# Security policy

Do not report real API keys, Wi-Fi passwords, or device dumps in a public issue.

For production devices, do not provision an OpenAI API key directly to the ESP32. Put the key behind a backend proxy with per-device authentication, quotas, revocation, and audit logging. Enable ESP32 secure boot and flash encryption when the hardware lifecycle supports them.

If a key was entered into an untrusted device or committed to Git, revoke it immediately in the OpenAI dashboard and issue a new key.
