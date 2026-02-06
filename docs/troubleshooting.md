# Troubleshooting Guide

## Common Issues

### WiFi Connection Problems

- **Symptoms:** Device won't connect, stuck in AP mode
- **Solutions:** Check SSID/password, verify 2.4GHz network, restart device, fully erase flash before uploading firmware
- **Debug:** Check serial logs for connection attempts and error codes

### Sensor Not Detected

- **Symptoms:** Temperature shows "---°F", water meter shows unconnected
- **Solutions:** Check wiring, verify pullup resistors (4.7kΩ for Dallas), replace sensor
- **Debug:** Serial monitor shows sensor detection messages on boot

### Pump Not Working

- **Symptoms:** Pump doesn't turn on, flow error detected
- **Solutions:** Check wiring, verify pump power, check for blockages
- **Debug:** Monitor pump state via web UI and serial logs

### Web UI Not Loading

- **Symptoms:** Can't access web interface, connection refused
- **Solutions:** Check device IP, verify WiFi connection, restart device
- **Debug:** Serial monitor shows web server status and IP

---

## Debug Tools

### Serial Monitor

- Baud rate: 115200
- Provides detailed system logs
- Shows sensor readings, pump state, errors/warnings
- Detailed state transitions and timing information
- Displays error messages and warnings with severity levels

### Web UI Debug

- Use browser developer tools
- Check network requests and responses (Network tab for API requests)
- Monitor console for JavaScript errors
- Verify API endpoint responses at `/sensor_status`, `/logs`

### System Logs

- Access via `/logs` endpoint
- Filter by severity level
- Search for specific events
- Export for offline analysis

---

## Performance Issues

### Memory Problems

- **Symptoms:** Random reboots, crashes, slow response
- **Solutions:** Reduce buffer sizes, optimize String usage, free unused memory
- **Monitoring:** Check heap size in web UI system status, monitor memory allocation

### Network Issues

- **Symptoms:** Slow web UI, dropped connections
- **Solutions:** Check WiFi signal strength, reduce polling frequency
- **Monitoring:** Monitor connection quality, check for interference

---

## Recovery Procedures

### Soft Reset

Restart via web UI `/reboot` or power cycle. Clears temporary state, preserves settings. First step for most issues.

### Factory Reset

**Hardware:** Hold door manual switch for 20 seconds during boot. WiFi LED rapid blinks to confirm. Clears all settings and WiFi credentials.

**Software:** Settings page -> Factory Reset button -> Confirm.

**Manual:**

```bash
pio run --target erase
pio run --target uploadfs
```

**Reset Behavior:**

- Clears all settings in `user_settings.json`
- Removes WiFi credentials
- Sets AP mode active
- Reverts to default values for all configurable parameters
- Preserves firmware and web UI files
- Creates new AP network `CoopController`

### Firmware Recovery

Use USB programming if OTA fails. Requires physical access to device. Last resort for bricked devices.

---

## Performance Optimization

### Memory Management

- Use static allocation where possible
- Avoid dynamic memory fragmentation
- Monitor heap usage regularly
- Implement memory leak detection

### Network Optimization

- Use connection pooling
- Implement request caching
- Optimize JSON payload sizes
- Compress static assets (already implemented via gzip)

### Power Management

- Use deep sleep when appropriate
- Optimize sensor reading frequency
- Implement power-saving modes
- Monitor power consumption

---

## Future Considerations

### Scalability

- Design for multiple coop units
- Implement centralized management
- Consider cloud integration
- Plan for data analytics

### Extensibility

- Plugin architecture for sensors
- Modular component design
- API versioning strategy
- Configuration migration
- Design for future mobile app and Google Messaging Service

### Maintenance

- Automated health checks
- Remote diagnostics
- Predictive maintenance
- Update automation
