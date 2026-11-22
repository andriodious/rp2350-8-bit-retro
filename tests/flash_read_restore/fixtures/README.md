# Flash read regression fixture

`mem-before-restore.c` is the exact flash wrapper used by diagnostic C64 image
`af26b56bf4ad537e`, before the read-command/read-format restoration fix. It is a
host-test input only and is never compiled into firmware.

Source baseline: `60c87fa9850314bf41db86ac7977e905874a6fb0`, plus the runtime
observer hooks recorded in the original diagnostic source patch. The wrapper
restores M0 timing only, reproducing the captured quad-to-serial read downgrade.

SHA-256 of the complete fixture:
`9d67d9c48b0118b0fb8cfda8a6412978a9815d0308175627a79c21ad116d40e4`.

The test compiles this preserved callback and the current production `mem.c`
against the same SDK/flash mocks. Its default baseline lives in this repository
so a fresh GitHub checkout can reproduce the old failure without another local
checkout. `--baseline` remains available for an explicit alternative source.
