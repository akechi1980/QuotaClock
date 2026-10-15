import json
from pathlib import Path
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from server import create_server, validate


class ServiceTests(unittest.TestCase):
    def test_boundaries_and_invalid_types(self):
        self.assertEqual(validate([0, 50, 100]), [0, 50, 100])
        for values in [[True, 0, 0], [1.5, 0, 0], [0, 0], [0, 0, 101], [0, -1, 0], '0,0,0']:
            with self.subTest(values=values), self.assertRaises(ValueError):
                validate(values)

    def exercise(self, **kwargs):
        server = create_server(port=0, **kwargs)
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        return f'http://127.0.0.1:{server.server_port}'

    def test_http_payload(self):
        base = self.exercise(values=[0, 50, 100])
        with urllib.request.urlopen(base + '/api/progress') as response:
            self.assertEqual(response.headers['Cache-Control'], 'no-store')
            self.assertEqual(json.load(response), {'progress': [0, 50, 100], 'simulation': True})

    def test_file_updates_and_invalid_file(self):
        with tempfile.TemporaryDirectory() as temporary:
            file = Path(temporary) / 'progress.json'
            file.write_text('{"progress":[1,2,3]}')
            base = self.exercise(file=file)
            with urllib.request.urlopen(base + '/api/progress') as response:
                self.assertEqual(json.load(response)['progress'], [1, 2, 3])
            file.write_text('{"progress":[false,2,3]}')
            with self.assertRaises(urllib.error.HTTPError) as error:
                urllib.request.urlopen(base + '/api/progress')
            self.assertEqual(error.exception.code, 503)
            error.exception.close()

    def test_no_write_endpoint(self):
        base = self.exercise()
        request = urllib.request.Request(base + '/api/progress', data=b'{}', method='POST')
        with self.assertRaises(urllib.error.HTTPError) as error:
            urllib.request.urlopen(request)
        self.assertEqual(error.exception.code, 501)
        error.exception.close()


if __name__ == '__main__':
    unittest.main()
