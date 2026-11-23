# Verified four-machine Wi-Fi startup fix

The user accepted the combined test build on 23 November 2026: “Much better. Works now exactly as expected.” This followed the request to check reconnection with Wi-Fi available and unavailable on all four machines. No individual status samples were supplied.

The exact tested UF2 is retained for v0.9.2, without rebuilding. See [verification.json](verification.json) for source, checksum, image identities and host checks. Bluetooth startup waits for DHCP or the full 30 seconds when credentials exist, and starts immediately without credentials. The earlier verified five-title paging controls are included. C64 static headroom is 26,808 bytes; performance options and diagnostics match the paging checkpoint.

Previous checkpoints and v0.9.1 remain unchanged.
