set -e
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HOST="$(cd "$(dirname "$0")" && pwd)"
mkdir -p /tmp/c64host
CORE="$ROOT/deps/frank-c64/src"
MACHINE="$ROOT/src/machines/c64"
TEST_SOURCE=main.cpp
[ "${1:-}" != t64 ] || TEST_SOURCE=t64.cpp
[ "${1:-}" != media ] || TEST_SOURCE=media.cpp
cc -fsanitize=address,undefined -c "$MACHINE/disk_file.c" -o /tmp/c64host/disk.o
cc -fsanitize=address,undefined -I"$MACHINE" -c "$MACHINE/t64.c" -o /tmp/c64host/t64.o
c++ -std=c++20 -O1 -g -fsanitize=address,undefined -DFRODO_RP2350 -DPSRAM_MAX_FREQ_MHZ=125 -I"$HOST" -I"$MACHINE" -I"$CORE" -I"$CORE/rp2350" "$HOST/$TEST_SOURCE" "$HOST/arch.cpp" /tmp/c64host/disk.o /tmp/c64host/t64.o "$MACHINE/display.cpp" "$MACHINE/crt.cpp" "$CORE"/rp2350/{C64_rp2350,Prefs_rp2350,ROM_data,SID_rp2350,Tape_stub}.cpp "$CORE"/{CPUC64,VIC,CIA,IEC,Cartridge,REU,1541d64,1541gcr,CPU1541,VIA}.cpp -o /tmp/c64host/test
ASAN_OPTIONS=detect_leaks=0 /tmp/c64host/test "$ROOT/discs/elite-commodore-64-gma86-pal.d64"
