# Remote Testing Guide

This walks through setting up local test servers for each of the four remote
protocols (SSH, Telnet, FTP, TFTP), so you can exercise
`--protocol ssh|telnet|ftp|tftp` and the Qt **Connect to Remote** dialog
without needing real hardware first. Commands below are for
Debian/Ubuntu-based systems; adjust package names for other distributions.

> **Security note:** Telnet and (unauthenticated) FTP/TFTP send credentials
> and data in plain text, and TFTP has no authentication at all. The setups
> below are for an isolated test machine or VM only — never expose these
> services to an untrusted network. On a real card, prefer SSH wherever
> possible.

All commands assume you're testing against `127.0.0.1` (the same machine).
Swap in your card's real address once you've confirmed each flow works
locally.

---

## SSH

**1. Install and start the server**

```bash
sudo apt install openssh-server
sudo systemctl start ssh      # or: sudo /usr/sbin/sshd -D &
```

**2. Set up passwordless key-based login** (matches how `remote_ssh_scan()`
connects — it uses `BatchMode=yes`, so it never prompts for a password)

```bash
ssh-keygen -t ed25519 -f ~/.ssh/id_ed25519 -N ""
cat ~/.ssh/id_ed25519.pub >> ~/.ssh/authorized_keys
chmod 700 ~/.ssh && chmod 600 ~/.ssh/authorized_keys
```

**3. Verify plain SSH works first**

```bash
ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new \
    localhost "echo it works"
```

**4. Deploy the binary, then monitor it**

```bash
scp build/app_top_monitoring localhost:/usr/local/bin/

./build/app_top_monitoring --protocol ssh --host 127.0.0.1 \
    --user "$(whoami)" --once
```

If your test sshd runs on a non-default port (common when testing
alongside a real sshd on the same box), add `--port 2222` (or whichever
port you used) to the command above and to the `ssh -p ...` check in step 3.

---

## Telnet

Telnet automation in NEO is best-effort: it scripts a plain login (username,
then optionally a password, then the command, then `exit`) and reads
whatever comes back. It expects a simple login prompt followed directly by a
shell — no exotic terminal negotiation.

**1. Install a telnet server**

```bash
sudo apt install telnetd inetutils-inetd
sudo systemctl start inetutils-inetd
```

If your distribution packages a different telnet daemon (e.g.
`xinetd` + `telnetd`, or `busybox telnetd` on an embedded image), any of
them work as long as they present a standard `login:` prompt.

**2. Verify plain telnet works first**

```bash
telnet 127.0.0.1
# type your username, password if prompted, then `exit`
```

**3. Deploy the binary, then monitor it**

```bash
scp build/app_top_monitoring localhost:/usr/local/bin/

./build/app_top_monitoring --protocol telnet --host 127.0.0.1 \
    --user "$(whoami)" --password "yourpassword" --once
```

If the fetch comes back with "no valid data was found," telnet into the
target manually (step 2) and confirm the login sequence really does drop
straight into a shell with no extra prompts (e.g. no "Press any key," no
MOTD requiring a keypress) — those are the most common causes of a mismatch.

---

## FTP

NEO uploads over FTP using `curl`, so anything `curl -T file ftp://...`
can reach, NEO can deploy to.

**1. Install and start a server**

```bash
sudo apt install vsftpd
sudo systemctl start vsftpd
```

**2. Allow local user uploads** (vsftpd defaults to read-only for security)

Edit `/etc/vsftpd.conf`:

```ini
write_enable=YES
local_enable=YES
```

Then restart: `sudo systemctl restart vsftpd`

**3. Verify plain curl works first**

```bash
echo "test upload" > /tmp/test.txt
curl -T /tmp/test.txt ftp://"$(whoami)":yourpassword@127.0.0.1/test.txt
```

**4. Deploy with NEO**

```bash
./build/app_top_monitoring --protocol ftp --host 127.0.0.1 \
    --user "$(whoami)" --password "yourpassword" \
    --local-file build/app_top_monitoring
```

Omit `--user`/`--password` entirely to test anonymous upload, if your
server allows it.

---

## TFTP

NEO uploads over TFTP using the `tftp` (tftp-hpa) client's non-interactive
`-c put` mode. TFTP is UDP-based and has no authentication or reliable
"connection refused" — if the server isn't listening, NEO's internal
20-second timeout will eventually kill the stuck client and report a
timeout rather than an instant failure.

**1. Install and start a server**

```bash
sudo apt install tftpd-hpa
sudo mkdir -p /srv/tftp
sudo chmod 777 /srv/tftp      # test-only; tighten this for anything real
sudo systemctl start tftpd-hpa
```

**2. Verify plain tftp works first**

```bash
echo "test upload" > /tmp/test.txt
tftp -m binary 127.0.0.1 -c put /tmp/test.txt test.txt
ls /srv/tftp/test.txt
```

**3. Deploy with NEO**

```bash
./build/app_top_monitoring --protocol tftp --host 127.0.0.1 \
    --local-file build/app_top_monitoring
```

---

## Testing from the Qt app

Once any of the above works from the CLI, the same target/credentials work
in the Qt app: **Connect to Remote** → pick the protocol → fill in the same
host/port/user/password/file fields → **Connect && Fetch** (SSH/Telnet) or
**Deploy** (FTP/TFTP). A successful SSH/Telnet fetch replaces the main
table's contents with the card's process list; use **Back to Local** (in the
status bar) to return to live local monitoring.

---

## Moving to real hardware

Once local testing works, pointing NEO at a real card just means:

1. The card runs the matching **server** (`sshd`, a telnet daemon, an FTP
   server, or a TFTP server) — most embedded Linux images already have one
   or more of these available or easy to enable.
2. For SSH/Telnet **monitoring**, `app_top_monitoring` must already exist on
   the card (deploy it there first via FTP/TFTP, or by any other means —
   `scp`, a firmware image, etc.). `--remote-bin` lets you point at a
   different path/name than the default if needed.
3. Swap `127.0.0.1` for the card's real address, and drop any `--port`
   override you only needed to dodge a conflicting local service.

---

## Troubleshooting

| Symptom | Likely cause |
| --- | --- |
| `SSH command failed (exit 255)` | Wrong host/port, sshd not running, or key not in `authorized_keys` |
| `SSH connection failed` immediately | Firewall blocking the port, or nothing listening |
| Telnet: "no valid data was found" | Login sequence didn't match (extra prompt, wrong password, or the shell isn't reached directly) |
| `TFTP transfer failed` after ~20s | Nothing listening on the TFTP port, or a firewall is dropping the UDP packets silently |
| `FTP upload failed` | Wrong credentials, `write_enable=NO`, or a permissions issue on the server's upload directory |
| Deploy succeeds but SSH/Telnet monitoring still fails | Binary deployed but not executable (`chmod +x`) or not on `$PATH` — try `--remote-bin /full/path/to/app_top_monitoring` |
