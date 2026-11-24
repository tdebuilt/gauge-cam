#!/usr/bin/env python3
"""Checks that web/openapi.json describes exactly the routes of src/web.cpp."""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
source = open(os.path.join(ROOT, "src", "web.cpp")).read()
routes = {(path, method.lower()) for path, method in
          re.findall(r'server\.on\("(/api[^"]*)", HTTP_(GET|POST)', source)}
doc = json.load(open(os.path.join(ROOT, "web", "openapi.json")))
described = {(path, method) for path, ops in doc["paths"].items() for method in ops}
missing, extra = sorted(routes - described), sorted(described - routes)
for path, method in missing:
    print(f"FAIL {method.upper()} {path}: in web.cpp, not in openapi.json")
for path, method in extra:
    print(f"FAIL {method.upper()} {path}: in openapi.json, not in web.cpp")
if missing or extra:
    sys.exit(1)
print(f"OK   openapi.json describes the {len(routes)} API routes")
