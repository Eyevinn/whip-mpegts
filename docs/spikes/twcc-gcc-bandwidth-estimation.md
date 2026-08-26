# Spike: transport-wide-cc availability & bandwidth-estimation API in webrtcbin (issue #43)

Investigation only — no production code changes. Goal: pin down the concrete API surface so
the implementation sub-issues can proceed (#44 wire the GCC estimator, #45 apply the estimate
to the encoder, #46 add an opt-in flag).

## Summary / verdict

- **TWCC on the wire: NOT confirmed from static inspection.** The current `webrtcbin` setup does
  nothing TWCC-specific: it does not add the `transport-wide-cc` RTP header extension to the
  payloaders, does not set any TWCC property, and does not inspect the negotiated SDP. Whether
  the extension is actually negotiated depends on GStreamer's `webrtcbin` defaults plus what the
  remote WHIP server offers back — neither of which can be verified without a live SDP capture.
  Per the issue's own guidance, this is raised as a **follow-up/risk to resolve before #44**
  (see Open questions).
- **Estimator element/signal: confirmed.** The mechanism is the `rtpgccbwe` element (Google
  Congestion Control) plugged into `webrtcbin` via the **`request-aux-sender`** signal; the app
  reacts to the **`notify::estimated-bitrate`** notification and re-targets the encoder. Bitrate
  values are in **bits per second (bps)**. The estimate/notify fires from the element's own
  **streaming thread**, not the main loop — so the encoder update must be thread-safe.
- **Version requirement: gap identified.** `rtpgccbwe` lives in the Rust plugin set
  (`gst-plugins-rs`, plugin `rsrtp`). This repo's `Dockerfile` targets Debian trixie
  (GStreamer **1.26.2**) but **does not install any gst-plugins-rs package**, and Debian trixie
  does **not** ship one. So on the current image the element is **absent** and must be added
  (extra package or build-from-source) before #44/#45 can run.
- **Estimator inputs: confirmed.** `min-bitrate` / `estimated-bitrate` (start) / `max-bitrate`,
  all in bps. `config.videoEncodeBitrate` (default 2000, kb/s per `Config.h:24`,
  `Config.h:120`) maps to the start bitrate as `videoEncodeBitrate * 1000` bps, mirroring the
  existing VP8 `target-bitrate` conversion at `Pipeline.cpp:79-82`.

---

## 1. TWCC on the wire

**Current code (static findings):**

- `webrtcbin` is created at `Pipeline.cpp:54` (`makeElement(ElementLabel::WEBRTC_BIN, "webrtcbin")`)
  and configured at `Pipeline.cpp:142-151` — only `name`, `stun-server`, `bundle-policy`
  (`GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE`), and `latency` are set. Nothing TWCC-related.
- Transceivers are configured in `onNegotiationNeeded()` at `Pipeline.cpp:611-623`: direction
  `SENDONLY`, `fec-type=NONE`, `do-nack=TRUE`. There is **no** call to add the
  `transport-wide-cc` header extension (e.g. via a payloader `add-extension` /
  `GstRTPHeaderExtension`), and no TWCC property is touched.
- Payloaders are `rtph264pay` / `rtpvp8pay` (`Pipeline.cpp:35`). The offer is created at
  `Pipeline.cpp:625-626` and serialized/sent verbatim at `Pipeline.cpp:642-645`; the code never
  parses the offer or the answer's `a=extmap` lines to check for
  `http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01`.

**Why this can't be confirmed statically:** whether TWCC ends up in the SDP is a function of
(a) GStreamer `webrtcbin`/payloader defaults for the pinned version and (b) the remote WHIP
server echoing the extension and sending TWCC RTCP feedback. GStreamer's RTP stack does
implement the TWCC header extension and the corresponding RTCP feedback packet format
(RTP-level TWCC seqnum + `transport-cc` RTCP), but the mailing-list guidance is that the
application generally has to ensure the extension is added to the payloader/session for it to
be negotiated — it is not guaranteed "for free" for an arbitrary sendonly pipeline. That echo
behaviour is a property of the live peer, invisible to static analysis.

**Action:** capture a live offer/answer (e.g. `GST_DEBUG=webrtcbin:5` or dumping the SDP text
that `Pipeline.cpp:642-643` already builds) against the target WHIP server and grep the
`a=extmap` / `a=rtcp-fb ... transport-cc` lines. Until then, treat TWCC-on-the-wire as
**unconfirmed**.

Spec: draft-holmer-rmcat-transport-wide-cc-extensions-01 (TWCC header extension + `transport-cc`
RTCP feedback).

## 2. Estimator element / signal

**Mechanism:** GStreamer's WebRTC GCC approach uses the `rtpgccbwe` element requested through
`webrtcbin`'s **`request-aux-sender`** signal.

- Signal: `request-aux-sender` on `webrtcbin`. Callback signature:
  `GstElement* cb(GstElement* webrtcbin, GstWebRTCDTLSTransport* transport, gpointer user_data)`.
  The callback **returns the aux-sender element** (an `rtpgccbwe`) that `webrtcbin` inserts on
  the send path for that transport. (GStreamer `webrtc` plugin docs.)
- The application then connects to the estimate notification:
  **`g_signal_connect(gccbwe, "notify::estimated-bitrate", ...)`**. The GCC element sets its
  **`estimated-bitrate`** property each time it produces a new estimate; the handler reads that
  property and re-targets the encoder. (GStreamer `rsrtp`/`rtpgccbwe` docs.)

**Units:** all `rtpgccbwe` bitrate properties are in **bits per second (bps)**.

**Threading:** `rtpgccbwe` runs **its own streaming thread on its srcpad** (it also does pacing),
so `notify::estimated-bitrate` fires on that **streaming thread, not the GLib main loop**.
Implication for #45: the encoder `g_object_set(... "bitrate"/"target-bitrate" ...)` from the
callback happens off the main thread — fine for GStreamer property sets, but any shared state
the handler touches must be synchronized, and the handler must not block the pacer thread.

Docs: rtpgccbwe — https://gstreamer.freedesktop.org/documentation/rsrtp/rtpgccbwe.html ;
webrtcbin `request-aux-sender` — https://gstreamer.freedesktop.org/documentation/webrtc/index.html

## 3. Version requirement

- Pinned base image: `FROM debian:trixie` (`Dockerfile:2`, `Dockerfile:13`). Trixie ships
  GStreamer **1.26.2** (e.g. `gstreamer1.0-plugins-bad 1.26.2-3+deb13u3`).
- Installed GStreamer packages (`Dockerfile:5`, `Dockerfile:16`): `gstreamer1.0-plugins-bad`,
  `-good`, `-libav`, `-plugins-rtp`, `-plugins-ugly`, `-nice` (+ `-tools` in the runtime stage).
  `CMakeLists.txt:11-13` links only `gstreamer-1.0`, `gstreamer-webrtc-1.0`, `gstreamer-sdp-1.0`.
- **`rtpgccbwe` is NOT in any of those.** It belongs to the Rust plugin set (`gst-plugins-rs`,
  plugin `rsrtp`, package name upstream `gst-plugin-rtp`). Debian trixie does **not** provide a
  `gstreamer1.0-plugins-rs` package, and none is installed here.

**Conclusion:** on the current image the estimator element is **unavailable**. #44 must first
add it — either by installing a distro/third-party `gst-plugins-rs` build or building `rsrtp`
from source — and add a runtime guard (`gst_element_factory_find("rtpgccbwe")`) so the opt-in
flag (#46) degrades gracefully when the plugin is missing. (The `webrtcbin`/`request-aux-sender`
API itself is present in 1.26, so no core-version bump is needed — only the plugin.)

## 4. Estimator inputs and mapping

`rtpgccbwe` properties (bps), with the documented defaults:

| Property | Meaning | Default |
|----------|---------|---------|
| `min-bitrate` | floor the estimate may drop to | 1 000 bps |
| `estimated-bitrate` | current estimate; **settable before start to seed the start bitrate** | 2 048 000 bps |
| `max-bitrate` | ceiling the estimate may rise to | 8 192 000 bps |

Mapping to config:

- Start / seed = `config.videoEncodeBitrate * 1000` bps. `videoEncodeBitrate` defaults to `2000`
  and is in **kb/s** (`Config.h:24`, `Config.h:120`; used as kb/s for x264 `bitrate` at
  `Pipeline.cpp:95-96` and multiplied by 1000 to bps for VP8 `target-bitrate` at
  `Pipeline.cpp:79-82`). So start = `2000 * 1000 = 2 000 000` bps by default (close to the
  element's own 2 048 000 default).
- `max-bitrate` should be the configured bitrate treated as the ceiling (start == max is a safe
  default so GCC only ever backs off from the operator-requested rate), or a separate future
  config knob if we want head-room above the nominal rate.
- `min-bitrate` should be a sane floor well above 1 kbps for video (e.g. a few hundred kb/s);
  worth a dedicated config field rather than the element default.
- On each `notify::estimated-bitrate`, convert the reported bps to the encoder's expected unit:
  x264enc `bitrate` is **kb/s** (`Pipeline.cpp:95`) → `estimate_bps / 1000`; vp8enc
  `target-bitrate` is **bps** (`Pipeline.cpp:80`) → use `estimate_bps` directly.

---

## Open questions / follow-ups

1. **Live-SDP TWCC confirmation (blocking for #44).** Cannot be settled by static inspection or
   in this build-less environment. Before wiring GCC, capture a real offer/answer against the
   target WHIP server and confirm both the `transport-wide-cc` `a=extmap` line and a
   `transport-cc` RTCP feedback line are present, and that feedback is actually received. If
   TWCC is not negotiated, GCC has no input signal and #44 should not proceed until the
   payloader/transceiver is made to add the extension explicitly.
2. **Plugin packaging (blocking for #44).** Decide how `rtpgccbwe` gets onto the image (distro
   package vs. third-party `gst-plugins-rs` build vs. source build) and pin its version; add the
   `gst_element_factory_find` availability guard.
3. **rsrtp version pin.** The GStreamer version is pinned (1.26.2) but the `gst-plugins-rs`
   version would be independent; pin it explicitly once the packaging route is chosen.
4. **Exact `estimated-bitrate` default vs. seed.** Confirmed defaults above are from the current
   docs; re-verify against the specific `gst-plugins-rs` build we ship, since defaults can shift
   between releases.

## Implications for #44 / #45 / #46

- **#44 (wire GCC estimator):** connect `request-aux-sender` on the `webrtcbin` created at
  `Pipeline.cpp:54`, return an `rtpgccbwe` with `min/estimated(start)/max-bitrate` in bps.
  Gate on `gst_element_factory_find("rtpgccbwe")`. **Do not merge until follow-up #1 (live TWCC)
  and #2 (plugin present) are resolved** — GCC is inert without TWCC feedback.
- **#45 (apply estimate to encoder):** handle `notify::estimated-bitrate`; branch on
  `config.vp8_` to write vp8enc `target-bitrate` (bps) or x264enc `bitrate` (kb/s), reusing the
  unit conventions already at `Pipeline.cpp:79-101`. Handler runs on the estimator's streaming
  thread — keep it non-blocking and clamp to `[min, max]`.
- **#46 (opt-in flag):** add a `Config` field (e.g. `gcc_`) alongside the existing bools in
  `Config.h:106-128`, wired through `main.cpp` arg parsing; when off, skip the
  `request-aux-sender` connection entirely so behaviour is unchanged. Also short-circuit to
  "off" with a logged warning when the plugin is missing.
