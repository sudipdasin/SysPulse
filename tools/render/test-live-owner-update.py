#!/usr/bin/env python3
"""DISPOSABLE container only. Real owner-signed feed, actual installed broker and APT.
A lower Debian revision of the exact public package is a test-only baseline, NOT a prior release.
No production signing secrets, key substitution, URL override, or simulated installation.
"""
import base64, hashlib, json, os, pathlib, socket, subprocess, sys, tempfile, urllib.request

VERSION = "3.0.0~alpha26~render1"
BASELINE = "3.0.0~alpha26"
DIGEST = "1ed875f39d95531a79a8685019d6409505c8cff05bed98186a37e8611037d70f"
KEY_DIGEST = "1d16b08d5bf50ba03cd8f294a94ef7f5259f9c5f4795a39134e9fde4b5968dc6"
NAME = "syspulse-3.0.0-alpha.26-x86_64-ubuntu-22.04-render-eval.deb"
URL = "https://raw.githubusercontent.com/sudipdasin/SysPulse/updates-alpha/pool/syspulse-3.0.0-alpha.26_amd64.deb"

def run(*args, timeout=90):
    result = subprocess.run(args, check=True, capture_output=True, text=True, timeout=timeout)
    return result.stdout.strip()

def version():
    return run("/usr/bin/dpkg-query", "-W", "-f=${Version}", "syspulse")

def discover(expected):
    observed = run("/usr/sbin/runuser", "-u", "tester", "--", "/usr/bin/syspulse-apt-update", "check", "alpha")
    assert observed == expected, observed
    print("Unprivileged live HTTPS discovery: " + observed, flush=True)

def main():
    if os.geteuid() != 0 or sys.argv[1:] != ["--disposable-container"] or os.environ.get("SYSPULSE_DISPOSABLE_TEST") != "owner-feed-alpha26" or not pathlib.Path("/candidate").is_dir() or not pathlib.Path("/upgrade/test-only.gpg").is_file():
        raise SystemExit("Refusing non-disposable environment")
    assert version() == VERSION
    out = pathlib.Path("/verified-production")
    out.mkdir(mode=0o755)
    package = out / NAME
    with urllib.request.urlopen(URL, timeout=30) as response:
        assert response.geturl() == URL, "Unexpected redirect"
        content = response.read(1152327)
    assert len(content) == 1152326 and hashlib.sha256(content).hexdigest() == DIGEST
    package.write_bytes(content)
    assert run("/usr/bin/dpkg-deb", "-f", str(package), "Version") == VERSION
    assert run("/usr/bin/dpkg-deb", "-f", str(package), "Package") == "syspulse"
    assert run("/usr/bin/dpkg-deb", "-f", str(package), "Architecture") == "amd64"
    # Actual immutable owner-signed DEB, not whatever bytes a later rebuild produced.
    print("Canonical feed package downloaded; exact published SHA-256 verified", flush=True)
    with tempfile.TemporaryDirectory(prefix="syspulse-live-test-") as tmp:
        tree = pathlib.Path(tmp) / "tree"
        run("/usr/bin/dpkg-deb", "-R", str(package), str(tree))
        key = tree / "usr/share/syspulse/update-keys/alpha.gpg"
        assert hashlib.sha256(key.read_bytes()).hexdigest() == KEY_DIGEST
        assert (key.parent / "alpha.sha256").read_text().strip() == KEY_DIGEST
        control = tree / "DEBIAN/control"
        raw = control.read_text()
        old = "Version: " + VERSION + "\n"
        assert raw.count(old) == 1
        control.write_text(raw.replace(old, "Version: " + BASELINE + "\n"))
        prior = pathlib.Path(tmp) / "test-only-lower-revision.deb"
        run("/usr/bin/dpkg-deb", "--build", "--root-owner-group", str(tree), str(prior))
        # Only the control version is changed, to exercise the released update code.
        # Do not upload or distribute this synthetic baseline.
        run("/usr/bin/dpkg", "-i", str(prior))
        assert version() == BASELINE
        discover("AVAILABLE " + VERSION)
        parent, child = socket.socketpair()
        parent.settimeout(240)
        proc = subprocess.Popen(["/usr/bin/syspulse-admin-session"], stdin=child, stdout=child, stderr=subprocess.DEVNULL, close_fds=True)
        child.close()
        stream = parent.makefile("rb")
        def request(payload):
            parent.sendall(payload + b"\n")
            line = stream.readline(6*1024*1024+2)
            result = json.loads(line)
            return result, base64.b64decode(result["output"], validate=True)
        try:
            assert stream.readline(128) == b"SYSPULSE_ADMIN_V1\n"
            result, output = request(b"apt-clean")
            assert result["code"] == 0
            result, output = request(json.dumps(["update", "alpha", VERSION], separators=(",", ":")).encode())
            assert result["code"] == 0 and not result["truncated"] and output.strip() == ("INSTALLED " + VERSION).encode(), output.decode(errors="replace")[:512]
            assert version() == VERSION
            # The same already-authorized broker survives installing its replacement.
            result, output = request(b'["volume","/","standard"]')
            assert result["code"] == 0 and not result["truncated"]
            assert isinstance(json.loads(output), dict)
            stream.close()
            parent.close()
            assert proc.wait(timeout=10) == 0
        finally:
            stream.close()
            parent.close()
            if proc.poll() is None:
                proc.wait(timeout=250)
        discover("CURRENT " + VERSION)
    assert run("/usr/sbin/runuser", "-u", "tester", "--", "/usr/bin/sha256sum", "-c", "/home/tester/preference.sha256").endswith(": OK")
    assert run("/usr/sbin/runuser", "-u", "tester", "--", "/usr/bin/syspulse", "--version").endswith("3.0.0-alpha.26")
    (out / "SHA256SUMS").write_text(DIGEST + "  " + NAME + "\n")
    (out / "LIVE-UPDATE-STATUS.txt").write_text(
        "PASSED: actual owner-signed public HTTPS feed; canonical DEB digest; packaged key; ordinary-user discovery; "
        "actual APT upgrade through one broker after a cache clean, then protected volume inspection on that same broker; "
        "installed version, current status, preference preservation and EOF revocation. "
        "Baseline was a disposable lower Debian revision of the identical released binary payload, not a second published release. "
        "Broker launched as root in a container; real XFCE PolicyKit dialog, service-manager mutation and successive published releases still require testing.\n"
    )
    print("LIVE OWNER-SIGNED ONLINE UPGRADE THROUGH SAME BROKER PASSED", flush=True)

if __name__ == "__main__":
    main()
