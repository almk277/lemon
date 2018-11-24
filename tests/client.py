#!/usr/bin/env python3

import sys
import argparse
import urllib.request
import urllib.error
from http.client import HTTPConnection
from concurrent.futures import ThreadPoolExecutor

def parse_command_line():
    parser = argparse.ArgumentParser()
    parser.add_argument('-a', '--host', help='server host string')
    parser.add_argument('-p', '--port', type=int, help='server port')
    parser.add_argument('-t', '--thread_count', type=int, help='number of worker threads')
    parser.add_argument('-i', '--infinite', action='store_true', help='run infinite test loop')
    parser.add_argument('-u', '--urllib', action='store_true', help='use Python urllib')
    parser.add_argument('-v', '--verbose', action='store_true', help='print more messages')
    parser.add_argument('encoded_samples', nargs='*', help='samples to test', metavar='sample')
    args = parser.parse_args()
    result = {k: v for k, v in vars(args).items() if v}
    result['infinite'] = args.infinite
    return result

def good(no, method, path, expected_body, req_body = None):
    return no, method, path, req_body, (200, expected_body)

def bad(no, method, path, code):
    return no, method, path, None, (code, None)

def is_good(sample):
    _, _, _, _, (code, _) = sample
    return code == 200

class BinData(bytes):
    def __str__(self):
        return bytes.__str__(self) if len(self) < 16 \
          else bytes.__str__(self[:16]) + '...'

    __repr__ = __str__

RICH_BODY = BinData(range(256))
BIG_BODY = BinData(RICH_BODY * 4 * 1024)

REQUEST_SAMPLES = [
    good(0, 'GET',    '/index',         b'It works!'),
    good(1, 'GET',    '/query?key=val', b'key=val'),
    good(2, 'POST',   '/echo',          b'Test body', b'Test body'),
    good(3, 'POST',   '/echo',          RICH_BODY, RICH_BODY),
    # good(4, 'POST',   '/echo',          BIG_BODY, BIG_BODY),
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

class Client:

    host = ''
    port = 0
    thread_count = 1
    _debug = lambda *_: None
    url_prefix = ''
    samples = []
    runner = None

    def __init__(self, encoded_samples = [], host = 'localhost', port = 8080,
                 thread_count = 10, urllib = False, verbose = False):
        self.host = host
        self.port = port
        self.url_prefix = 'http://' + host + ':' + str(port)
        self.thread_count = thread_count
        self.runner = self.one_shot_session if urllib else self.long_session
        if encoded_samples:
            samples = map(sample_from_string, encoded_samples)
        else:
            generator = generate_sample_singletons if urllib else generate_sample_chains
            samples = generator(REQUEST_SAMPLES)
        self.samples = list(samples)
        if verbose:
            self._debug = print
    
    def one_shot_session(self, sample):
        for s in sample:
            # self._debug('one_shot_session', s)
            _, method, path, data, expected = s
            print(s)
            try:
                req = urllib.request.Request(self.url_prefix + path, data, method=method)
                with urllib.request.urlopen(req) as resp:
                    result = resp.status, resp.read()
            except urllib.error.HTTPError as e:
                result = e.code, None
            # self._debug('Got', result)
            if result != expected:
                return sample_to_string(sample)
        return None

    def long_session(self, sample):
        conn = HTTPConnection(self.host, self.port)
        for s in sample:
            self._debug('long_session', sample_repr(s))
            _, method, path, data, (expected_code, expected_body) = s
            conn.request(method, path, data)
            resp = conn.getresponse()
            code, body = resp.status, resp.read()
            self._debug('Got', result_repr((code, body)))
            if code != expected_code or (code == 200 and body != expected_body):
                return sample_to_string(sample)
        return None

    def run(self):
        with ThreadPoolExecutor(max_workers=self.thread_count) as executor:
            results = executor.map(self.runner, self.samples)
            return list(filter(None, results))


def main():
    args = parse_command_line()
    infinite = args.pop('infinite')
    client = Client(**args)
    
    if infinite:
        while True: client.run()
    
    errors = client.run()
    if errors:
        client._debug('Error count:', len(errors))
        sys.exit(errors)
    else:
        client._debug('OK')


if __name__ == "__main__":
    main()