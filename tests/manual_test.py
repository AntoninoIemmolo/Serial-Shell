#!/usr/bin/env python3
import os, time, signal, subprocess, sys

SOCAT = None
SERVER = None

def cleanup():
    global SOCAT, SERVER
    for p in [SERVER, SOCAT]:
        if p:
            try:
                p.terminate()
                time.sleep(0.1)
                p.kill()
                p.wait()
            except:
                pass
    for f in ['/tmp/mt_tty0', '/tmp/mt_tty1', '/tmp/mt_out.txt']:
        try:
            os.unlink(f)
        except:
            pass

def main():
    global SOCAT, SERVER
    cleanup()

    SOCAT = subprocess.Popen(
        ['socat', '-d', '-d',
         'PTY,link=/tmp/mt_tty0', 'PTY,link=/tmp/mt_tty1'],
        stderr=subprocess.DEVNULL)
    time.sleep(1)

    for p in ['/tmp/mt_tty0', '/tmp/mt_tty1']:
        if not os.path.exists(p):
            print(f"FAIL: {p} not created")
            sys.exit(1)

    out = open('/tmp/mt_out.txt', 'w')
    SERVER = subprocess.Popen(
        ['./piSerialServer', '/tmp/mt_tty0'],
        stdout=out, stderr=subprocess.STDOUT)
    time.sleep(1)

    if SERVER.poll() is not None:
        print("FAIL: server exited early")
        sys.exit(1)

    open('/tmp/mt_tty1', 'w').close()
    time.sleep(0.5)

    with open('/tmp/mt_out.txt') as f:
        banner = f.read()
    if not banner:
        print("FAIL: no startup banner")
        sys.exit(1)

    test_data = b"ABCDEFG\nhallo\nword\n"
    with open('/tmp/mt_tty1', 'ab', buffering=0) as f:
        f.write(test_data)

    time.sleep(1.5)
    out.close()

    with open('/tmp/mt_out.txt') as f:
        output = f.read()

    body = output[len(banner):]
    print("Server stdout:")
    print(repr(body))

    lines = body.replace('\r\n', '\n').replace('\r', '\n').strip().split('\n')
    bug = False
    for i, line in enumerate(lines):
        if line and line[0] == ' ':
            print(f"BUG: line {i} starts with space — staircase: {repr(line)}")
            bug = True

    for needle in [b"ABCDEFG", b"hallo", b"word"]:
        if needle.decode() not in body:
            print(f"BUG: missing {needle}")
            bug = True

    if bug:
        print("FAIL: staircase bug detected")
        sys.exit(1)
    else:
        print("OK — clean newlines, no staircase")

if __name__ == '__main__':
    try:
        main()
    finally:
        cleanup()
