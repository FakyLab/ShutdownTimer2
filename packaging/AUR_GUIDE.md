# Publishing ShutdownTimer to AUR (Arch Linux)

## Prerequisites

You need an Arch Linux system (or VM) with:
```bash
sudo pacman -S base-devel git cmake ninja qt6-base qt6-tools polkit glib2
```

## Step 1 — Create AUR account

Go to https://aur.archlinux.org and create an account.

## Step 2 — Set up SSH key for AUR

```bash
ssh-keygen -t ed25519 -C "your-email@example.com"
cat ~/.ssh/id_ed25519.pub
# Paste the output into your AUR account → My Account → SSH Public Key
```

## Step 3 — Compute the sha256sum

```bash
# Download the release tarball
wget https://github.com/FakyLab/ShutdownTimer/archive/v1.0.0.tar.gz
sha256sum v1.0.0.tar.gz
# Replace 'SKIP' in PKGBUILD and .SRCINFO sha256sums with the real hash
```

## Step 4 — Test the build locally

```bash
cd packaging/aur
makepkg -si
# Installs the package locally — verify it runs correctly
```

## Step 5 — Clone your AUR repo and push

```bash
git clone ssh://aur@aur.archlinux.org/shutdowntimer.git
cd shutdowntimer

# Copy PKGBUILD and .SRCINFO
cp /path/to/project/packaging/aur/PKGBUILD .
cp /path/to/project/packaging/aur/.SRCINFO .

# Update .SRCINFO (always regenerate before pushing)
makepkg --printsrcinfo > .SRCINFO

git add PKGBUILD .SRCINFO
git commit -m "Initial release v1.0.0"
git push
```

The package appears on AUR immediately — no review required.

## Users install with

```bash
# Using yay
yay -S shutdowntimer

# Or manually
git clone https://aur.archlinux.org/shutdowntimer.git
cd shutdowntimer
makepkg -si
```

## Updating for new releases

1. Update `pkgver` and `pkgrel` in PKGBUILD
2. Update `sha256sums` with new tarball hash
3. Regenerate: `makepkg --printsrcinfo > .SRCINFO`
4. Commit and push to the AUR repo
