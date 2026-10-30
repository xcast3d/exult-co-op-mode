#!/bin/sh
# Serves web/out on this machine only: http://127.0.0.1:8642/exult.html
# Nothing is cached, so a reload always gets the latest build.
cd "$(dirname "$0")/out" && exec python3 -c '
import http.server
class Handler(http.server.SimpleHTTPRequestHandler):
	def end_headers(self):
		self.send_header("Cache-Control", "no-store")
		super().end_headers()
http.server.ThreadingHTTPServer(("127.0.0.1", 8642), Handler).serve_forever()
'
