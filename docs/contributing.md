# Contributing & Collaboration Guidelines

## Development Workflow

### 1. Development

- Create feature branch from main
- Follow [coding standards](development-guide.md#coding-style)
- Write comprehensive tests
- Update documentation as needed
- Verify compilation: `pio run` (C++) and `cd web && npm run build` (Web UI)

### 2. Testing

- Run full test suite locally (`pio test`)
- Test on actual hardware when applicable
- Verify web UI functionality
- Check for memory leaks and performance issues

### 3. Submission

- Create pull request with clear description
- Link to relevant issues
- Include screenshots for UI changes
- Request review

---

## Code Review Standards

### Review Checklist

- [ ] Follows project [coding style](development-guide.md#coding-style)
- [ ] Tests are comprehensive and pass
- [ ] Documentation updated
- [ ] No hardcoded values (use SettingsManager)
- [ ] Error handling is robust
- [ ] Memory usage appropriate for ESP32
- [ ] Security considerations addressed
- [ ] Performance impact acceptable

### Process

- At least one approval required for merge
- Address all review comments before merging
- Use suggestions for minor improvements
- Discuss major changes in comments

---

## Branch Management

### Branch Naming

- `feature/description` - New features
- `fix/description` - Bug fixes
- `docs/description` - Documentation
- `refactor/description` - Code refactoring

### Merge Strategy

- Squash merge for feature branches
- Keep main branch history clean
- Tag releases appropriately
- Delete merged branches

---

## Git Commit Standards

```text
type(scope): description

[optional body]

[optional footer]
```

**Types:** `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`

**Examples:**

- `feat(pump): add flow error detection`
- `fix(web): resolve temperature display issue`
- `docs(api): update endpoint documentation`

---

## Issue Management

### Bug Reports

- Detailed reproduction steps
- System information and logs
- Screenshots if applicable
- Expected vs actual behavior

### Feature Requests

- Describe use case and benefits
- Implementation suggestions
- Impact on existing functionality
- Priority discussion

---

## Restricted or Sensitive Files

### Never Commit

- `data/user_settings.json` - WiFi credentials and API keys
- Any file containing API keys, tokens, or passwords

### API Keys & Secrets (stored in settings, not code)

- OpenWeather API key
- OpenAI API key
- Telegram bot token
- Email credentials
- Home Assistant MQTT credentials

### Access Control

- OTA update authentication
- Web UI access restrictions (optional HTTP Basic Auth)
- API endpoint rate limiting
- Secure password storage

### File Protection

```gitignore
data/user_settings.json    # Sensitive settings
build/                     # Build artifacts
*.bin / *.elf              # Binaries
.vscode/                   # IDE files
logs/ / *.log              # Log files
```

### Runtime Protection

- Validate all user inputs
- Sanitize file paths
- Check file sizes before upload
- Implement access controls

---

## Backup & Recovery

### Configuration

- Export settings via web UI or `/settings/backup` endpoint
- Store backups securely
- Test restore procedure via `/settings/restore`
- Document backup locations

### Firmware

- Keep previous firmware versions
- Document rollback procedure
- Test recovery via USB if OTA fails
- Maintain changelog with each release

---

## Community Resources

### Documentation

- Keep Agents.md and docs/ updated with changes
- Maintain API documentation
- Provide troubleshooting guides
- Create video tutorials

### Support

- GitHub issues for bug reports
- Wiki for detailed guides
- Discord/Slack for community support
- Regular release notes

### Contributing

- Welcome community contributions
- Provide clear contribution guidelines
- Recognize valuable contributors
