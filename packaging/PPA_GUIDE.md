# Publishing ShutdownTimer to Ubuntu PPA

## Step 1 — Create a GPG key

Run on Linux (Ubuntu/Debian recommended):

```bash
gpg --full-generate-key
```

Choose:
- Key type: RSA and RSA
- Key size: 4096
- Expiry: 0 (no expiry)
- Name: FakyLab
- Email: fakylab@proton.me  (must match Launchpad account)
- Comment: (leave blank)

Note your key ID:
```bash
gpg --list-keys
# Look for: pub   rsa4096 2026-03-21 [SC]
#                 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX  <- this is your key ID
```

Export to Launchpad keyserver:
```bash
gpg --keyserver keyserver.ubuntu.com --send-keys YOUR_KEY_ID
```

## Step 2 — Create Launchpad account

1. Go to https://launchpad.net
2. Create an account using the same email as your GPG key
3. Go to your profile → OpenPGP keys → import your key ID
4. Create a PPA: Your profile → Create a new PPA
   - Name: shutdowntimer
   - Display name: ShutdownTimer
   - Description: Schedule system shutdown with login screen messaging

## Step 3 — Install required tools

```bash
sudo apt install devscripts dput-ng debhelper cmake ninja-build \
    qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
    libgl-dev libglib2.0-dev libpolkit-gobject-1-dev pkg-config
```

## Step 4 — Configure dput

Create `~/.dput.cf`:
```ini
[fakylab-shutdowntimer]
fqdn = ppa.launchpad.net
method = ftp
incoming = ~fakylab/ubuntu/shutdowntimer
login = anonymous
allow_unsigned_uploads = 0
```

## Step 5 — Update packaging/debian/changelog

Replace the email and verify the version:
```
shutdowntimer (1.0.0-1) focal; urgency=medium

  * Initial release.

 -- FakyLab <fakylab@proton.me>  Sat, 21 Mar 2026 00:00:00 +0000
```

Repeat this block for each Ubuntu series you want to support:
- `focal` = Ubuntu 20.04
- `jammy` = Ubuntu 22.04
- `noble` = Ubuntu 24.04

## Step 6 — Build source package

From the project root:
```bash
# Copy debian/ directory to project root (Debian tools expect it there)
cp -r packaging/debian debian

# Build the source package (no binary, signed)
dpkg-buildpackage -S -sa -k YOUR_KEY_ID

# This produces:
#   ../shutdowntimer_1.0.0-1.dsc
#   ../shutdowntimer_1.0.0-1.debian.tar.xz
#   ../shutdowntimer_1.0.0-1_source.changes
```

## Step 7 — Upload to PPA

```bash
dput ppa:fakylab/shutdowntimer ../shutdowntimer_1.0.0-1_source.changes
```

Launchpad will:
1. Verify your GPG signature
2. Build the binary package on their servers (~30 min)
3. Publish to the PPA

## Step 8 — Users install with

```bash
sudo add-apt-repository ppa:fakylab/shutdowntimer
sudo apt update
sudo apt install shutdowntimer
```

## Releasing a new version

1. Update `packaging/debian/changelog` with the new version
2. Repeat steps 6-7
3. Tag the release on GitHub: `git tag v1.1.0 && git push origin v1.1.0`
