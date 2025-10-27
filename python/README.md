# SLON for Python

Lightweight SLON (Single Line Object Notation) parser and formatter for Python projects.

## Installation

Copy this folder into your project or install it with `pip` using a local path:

```bash
pip install -e path/to/slon/python
```

The package exposes `loads`, `load`, `dumps`, and `dump`, mirroring the JSON module surface.

## Usage

```python
from slon_python import loads, dumps

payload = "(status: ok, metrics: [12 | 34 | 56], generatedAt: 2024-03-01/18:22:10.001)"
data = loads(payload)

assert data["status"] == "ok"
json_ready = dumps(data)
```

Datetime literals (`YYYY-MM-DD/HH:MM:SS.mmm`) are converted into timezone-aware `datetime` objects in UTC.
