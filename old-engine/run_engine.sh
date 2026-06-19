#!/bin/bash
# The -u flag prevents the TimeoutException by disabling buffering
python3 -u "$(dirname "$0")/engine.py"