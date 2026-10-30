# Audio outputs

Open the island and click the current audio output, or use the output icon in
its hover player. Select a connected output to change the system default and
move existing application playback. Paired Bluetooth audio devices remain in
the list while disconnected; selecting one connects it first, then waits for
its audio output to appear. Operations have timeouts and report failures in the
picker. Other audio controls (including Plasma) remain usable; Atoll observes
the current server state instead of continually restoring its own preference.

The Media settings page lets you rename any output locally. For example, an
analog sound-card output can be named “Headphones · AUX”. Names and the last
selected physical non-Bluetooth fallback are saved in the user's Atoll config,
under `audio.outputNames` and `audio.fallbackOutput`. No addresses or sound-card
names belong in the source tree. Empty names restore the server's device name.

## Switching to a phone or another computer

With Bluetooth headphones selected, **Release for another device** pauses the
active MPRIS player, selects the last available wired output (or the first
available physical non-Bluetooth output), and disconnects the headphones from
this computer. Select them in the other device's audio/Bluetooth picker. To
come back, select the headphones in Atoll. If the other host refuses to yield,
disconnect them there first. Re-pairing is normally unnecessary.

Atoll never reconnects headphones just because they were disconnected. It does
not change BlueZ's system-wide retry policy or the headset's firmware. Losing
audio hardware is handled by the existing PipeWire/PulseAudio policy; Atoll
shows the resulting state. It never changes microphone routing or volume when
switching outputs. Effect/filter links are excluded from explicit stream moves;
applications with private routing may need their own output changed.

This is explicit connection switching, not simultaneous Bluetooth multipoint
or Apple's automatic switching. Apple's documented automatic switching requires
supported Apple devices signed in to the same Apple Account. Linux projects
such as LibrePods describe manufacturer-ID spoofing for two-host multipoint;
`airpodsctl` explicitly marks ownership switching as experimental and targeted
at AirPods Pro 2 USB-C. These techniques are not enabled by Atoll. In particular,
three simultaneously connected hosts and automatic Apple-style handoff are not
promised. Validate any optional protocol helper against the actual headphones
and firmware before deploying it.

References (checked 2026-10-30):

- https://support.apple.com/en-gb/104988
- https://github.com/librepods-org/librepods#vendorid-spoofing
- https://github.com/hacel/airpodsctl#experimental-multipoint-switching

## Dependencies and debugging

- `pactl` with JSON support (Arch: `libpulse`) and a running PulseAudio-compatible
  server, usually `pipewire-pulse`.
- BlueZ (`bluez` on Arch) for connecting/disconnecting already-paired headphones.
  Initial pairing is done with the desktop Bluetooth settings.
- No new compile-time dependencies beyond Atoll's Qt DBus dependency.

The session D-Bus API and `atollctl` support:

```sh
atollctl outputs            # current outputs, busy flag and error/status message
atollctl output '<id>'      # id from the list; asynchronous operation
atollctl release-audio
```

Commands enqueue a request; inspect `outputs` for its result. Device ids are
stable sink names or Bluetooth addresses, not transient PipeWire object ids.

Tests use a temporary fake `pactl` and an unavailable test Bluetooth bus, so they
do not reroute real audio. Run `ctest --test-dir build -R audio-outputs
--output-on-failure`. Hardware acceptance should additionally check running
playback through wired → Bluetooth → wired, release/reconnect, an unavailable
headset, switching from the desktop audio panel, and phone/Mac handoff in both
directions. A successful connection alone does not verify audible playback.
