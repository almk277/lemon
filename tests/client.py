#!/usr/bin/env python3

import sys
import argparse
import urllib.request
import urllib.error
from http.client import HTTPConnection
from concurrent.futures import ThreadPoolExecutor

host = 'localhost'
port = 8080
thread_count = 20
infinite = False
use_urllib = False
debug = lambda *_: None
input_sample_strings = []
url_prefix = ''

def parse_command_line():
    global host, port, thread_count, infinite, use_urllib
    global debug, input_sample_strings, url_prefix
    parser = argparse.ArgumentParser()
    parser.add_argument('-a', '--host', help='server host string')
    parser.add_argument('-p', '--port', type=int, help='server port')
    parser.add_argument('-t', '--thread_count', type=int, help='number of worker threads')
    parser.add_argument('-i', '--infinite', action='store_true', help='run infinite test loop')
    parser.add_argument('-u', '--urllib', action='store_true', help='use Python urllib')
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('sample', nargs='*', help='samples to test')
    args = parser.parse_args()
    if args.host:
        host = args.host
    if args.port:
        port = args.port
    if args.thread_count:
        thread_count = args.thread_count
    infinite = args.infinite
    use_urllib = args.urllib
    if args.verbose:
        debug = print
    input_sample_strings = args.sample
    url_prefix = 'http://' + host + ':' + str(port)

def good(no, method, path, expected_body, req_body = None):
    return no, method, path, req_body, (200, expected_body)

def bad(no, method, path, code):
    return no, method, path, None, (code, None)

def is_good(sample):
    _, _, _, _, (code, _) = sample
    return code == 200

RICH_BODY = bytes(range(256))
BIG_BODY = RICH_BODY * 4 * 1024

REQUEST_SAMPLES = [
    good(0, 'GET',    '/index',         b'It works!'),
    good(1, 'GET',    '/query?key=val', b'key=val'),
    good(2, 'POST',   '/echo',          b'Test body', b'Test body'),
    good(3, 'POST',   '/echo',          RICH_BODY, RICH_BODY),
    good(4, 'POST',   '/echo',          BIG_BODY, BIG_BODY),
    good(5, 'DELETE', '/index',         b'del'),
    bad(6, 'BAD', '/index',  400),
    bad(7, 'GET', '/bad',    404),
    bad(8, 'GET', '/oom',    500),
    bad(9, 'GET', '/notimp', 501),
]

def sample_from_string(sample_string):
    indexes = map(int, sample_string.split("-"))
    return list(map(lambda i: REQUEST_SAMPLES[i], indexes))

def sample_to_string(sample):
    return "-".join([str(n) for n, _, _, _, _ in sample])

def data_repr(data):
    return data if data is None or len(data) < 16 else data[:16] + b'...'

def result_repr(result):
    code, body = result
    return code, data_repr(body)

def sample_repr(sample):
    no, method, path, body, (code, expected_body) = sample
    return no, method, path, data_repr(body), (code, data_repr(expected_body))

def generate_sample_singletons(samples):
    return map(lambda x: [x], samples)

def generate_sample_chains(samples):
    for s in samples:
        yield [s]
    for s1 in filter(is_good, samples):
        for s2 in samples:
            yield [s1, s2]
    for s1 in filter(is_good, samples):
        for s2 in filter(is_good, samples):
            for s3 in samples:
                yield [s1, s2, s3]

def one_shot_session(sample):
    for s in sample:
        debug('one_shot_session', sample_repr(s))
        _, method, path, data, expected = s
        try:
            req = urllib.request.Request(url_prefix + path, data, method=method)
            with urllib.request.urlopen(req) as resp:
                result = resp.status, resp.read()
        except urllib.error.HTTPError as e:
            result = e.code, None
        debug('Got', result_repr(result))
        if result != expected:
            return sample_to_string(sample)
    return None

def long_session(sample):
    s = sample
    conn = HTTPConnection(host, port)
    for s in sample:
        debug('long_session', sample_repr(s))
        _, method, path, data, (expected_code, expected_body) = s
        conn.request(method, path, data)
        resp = conn.getresponse()
        code, body = resp.status, resp.read()
        debug('Got', result_repr((code, body)))
        if code != expected_code or (code == 200 and body != expected_body):
            return sample_to_string(sample)
    return None

def run_tests(runner, samples):
    with ThreadPoolExecutor(max_workers=thread_count) as executor:
        results = executor.map(runner, samples)
        return list(filter(None, results))


def main():
    parse_command_line()

    runner = one_shot_session if use_urllib else long_session
    if input_sample_strings:
        samples = map(sample_from_string, input_sample_strings)
    else:
        generator = generate_sample_singletons if use_urllib else generate_sample_chains
        samples = generator(REQUEST_SAMPLES)
    sample_list = list(samples)
    
    while True:
        errors = run_tests(runner, sample_list)
        if not infinite: break
    
    if errors:
        debug('Error count:', len(errors))
        sys.exit(errors)
    else:
        debug('OK')


if __name__ == "__main__":
    main()