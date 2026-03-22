# Security Policy

## Supported Versions

Only the latest release of Shutdown Timer receives security fixes.

| Version | Supported |
|---------|-----------|
| 1.0.x   | ✅ Yes    |

---

## Reporting a Vulnerability

If you discover a security vulnerability, **please do not open a public GitHub issue.**

Instead, report it privately via GitHub's [Security Advisories](https://github.com/FakyLab/ShutdownTimer/security/advisories/new) feature.

Please include:
- A description of the vulnerability
- Steps to reproduce
- Potential impact
- Any suggested mitigation (optional)

You can expect an acknowledgement within **72 hours** and a resolution or status update within **14 days**.

---

## Security Considerations

Shutdown Timer requires **administrator / root privileges** by design, since it needs to:
- Call the OS shutdown API
- Write to system-level registry keys (Windows) or protected filesystem paths (Linux/macOS)
- Create scheduled tasks / systemd services / LaunchAgents

### Auto-clear task security (Windows)
The Windows auto-clear feature creates a privileged Task Scheduler task. As a safeguard, the app **refuses to create this task** if the executable is not located inside `Program Files` or `Program Files (x86)`. This prevents a user-writable path + high-privilege task from being used as a privilege escalation vector.

### Auto-clear task security (Linux)
The Linux auto-clear feature creates a **systemd user service**, which runs at the privilege level of the logged-in user, not root. Only the message cleanup (which was written by root) requires elevated access — this is handled by the `--auto-clear` handler which runs with the same privileges the app was originally launched with.

### No network access
Shutdown Timer makes **no network connections** at runtime. The only outbound URL in the codebase is the GitHub releases page opened in the user's browser via `QDesktopServices::openUrl`.
