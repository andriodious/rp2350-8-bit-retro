# Wi-Fi setup

Existing saved Wi-Fi settings survive this project transition. For a new device,
connect the Presto over USB, copy `tools/secrets.example.py` to `secrets.py` in the
project root and fill in the network name/password. Keep that file private; Git
ignores it. The USB tool requires Python's `pyserial` package.

From the project root:

```sh
python3 tools/presto.py wifi secrets.py
python3 tools/presto.py net
```

The first command saves the network and restarts the Presto. Allow 10–20 seconds
to connect before asking for network status. Open `http://retro8bit.local/` or the IP
address reported by the second command from a browser on the same network. With
multiple Prestos, use each device's own IP address to avoid hostname ambiguity.
