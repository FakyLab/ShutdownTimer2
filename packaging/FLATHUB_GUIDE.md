# Publishing ShutdownTimer to Flathub

## Prerequisites

Install Flatpak tools:
```bash
sudo apt install flatpak flatpak-builder
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
flatpak install flathub org.kde.Sdk//6.8 org.kde.Platform//6.8
```

## Step 1 — Update the manifest commit hash

Before submitting, update `linux/flatpak/org.fakylab.ShutdownTimer.yaml`
with the actual commit hash for your release tag:

```bash
git rev-parse v1.0.0
# Copy the output hash into the manifest's 'commit:' field
```

## Step 2 — Test the build locally

```bash
cd linux/flatpak
flatpak-builder --force-clean --install --user build-dir org.fakylab.ShutdownTimer.yaml
flatpak run org.fakylab.ShutdownTimer
```

## Step 3 — Validate AppStream metadata

```bash
sudo apt install appstream
appstreamcli validate linux/appstream/org.fakylab.ShutdownTimer.appdata.xml
# Must show no errors (warnings are okay)
```

## Step 4 — Fork and submit to Flathub

1. Fork https://github.com/flathub/flathub on GitHub
2. Create a new branch: `git checkout -b new-app/org.fakylab.ShutdownTimer`
3. Create directory: `mkdir apps/org.fakylab.ShutdownTimer`
4. Copy the manifest:
   ```bash
   cp linux/flatpak/org.fakylab.ShutdownTimer.yaml \
      /path/to/flathub-fork/apps/org.fakylab.ShutdownTimer/
   ```
5. Commit and push, then open a PR against `flathub/flathub`

## Step 5 — Review process

Flathub reviewers will check:
- AppStream metadata completeness (screenshots, description, categories)
- Flatpak permissions (finish-args) are minimal and justified
- No unnecessary network access
- App runs correctly in the sandbox

Expected review time: 1-4 weeks.

## Step 6 — After acceptance

Users install with:
```bash
flatpak install flathub org.fakylab.ShutdownTimer
flatpak run org.fakylab.ShutdownTimer
```

Or through GNOME Software / KDE Discover directly.

## Updating for new releases

1. Update the `tag:` and `commit:` fields in the manifest
2. Open a PR against your accepted Flathub app repo
   (Flathub creates a dedicated repo for your app after acceptance)
