# Security policy

## Secrets

This repository must never contain:

- GitHub personal access tokens
- SSH private keys
- boot-image signing keys
- device factory keys or certificates
- any `*.pem`, `*.key`, `id_rsa*`, `id_ed25519*`

If a token or key is found:

1. Revoke/rotate it immediately.
2. Remove it from the working tree.
3. If it was committed, rewrite history before publishing.

## Reporting

Please open a private security advisory or contact the repository owner for
sensitive issues.
