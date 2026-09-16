# Spike: intermittent audio-behind-video A/V drift on SRT MPEG-TS ingest (issue #38)

Diagnostic / bug-triage only. Split from #22. A user reports occasional, non-deterministic
audio lagging video when ingesting an SRT MPEG-TS stream (mpegts / libx264 video, AAC audio)
and egressing over WHIP. The issue asks to reproduce with a controlled SRT source, examine the
exposed latency knobs (`--tsDemuxLatency`, `--jitterBufferLatency`, `--srtSourceLatency`,
`-d/--udpSourceQueueMinTime`), and inspect how PTS/DTS survive the decode/re-encode boundary in
`Pipeline.cpp`, then **document recommended settings or fix the timestamp handling. Do not scope
further until reproduction is confirmed.**

## Reproduction status

**Not confirmed in this environment.** A live reproduction needs a real SRT source pushing an
mpegts/AAC stream and a real WHIP endpoint + WebRTC receiver to observe lip-sync — none of which
exist in the build-less agent sandbox (the repo's own CI only builds the `Dockerfile`, it does
not run media). Per the issue's "do not scope further until reproduction is confirmed", this
spike therefore does **not** propose a code fix. It maps the drift hypotheses to specific code
locations, states which are consistent with an *audio-behind-video* symptom, and gives
recommended knob settings and a concrete live-repro procedure to confirm before any fix lands.

## Summary / verdict

- **No dropped-timestamp bug is visible from static inspection.** Nowhere in `Pipeline.cpp` are
  PTS/DTS read, cleared, offset, or re-stamped by hand. Every branch is a straight
  `gst_element_link_many(...)` chain (`Pipeline.cpp:443-664`); timestamps flow through the
  standard GStreamer elements (`tsdemux` → parse → optional decode/encode → payloader →
  `webrtcbin`). There is no `do-timestamp`, `set-timestamps`, or manual `GST_BUFFER_PTS`
  manipulation anywhere. So the "fix the timestamp handling" branch of the issue has **no single
  line to change** — the drift is a *latency / buffering asymmetry* between the audio and video
  branches, not a discarded timestamp. This is why the correctly-scoped deliverable is
  recommended settings, not a code edit.
- **The audio and video branches have very different, and unbounded, buffering depth.** Every
  `queue` in the pipeline is created with all three `max-size-*` limits set to `0`
  (`Pipeline.cpp:762-767`), i.e. **unbounded**. The video transcode branch is a much longer /
  higher-latency chain (decode → `videoconvert` → `x264enc` → caps → payloader) than the audio
  branch, and on the *transcode* path the two branches accumulate different amounts of latency
  before `webrtcbin`. Because the payload queues cannot back-pressure or drop, a transient stall
  on one branch is absorbed as a permanent offset rather than being re-synced — exactly the
  "occasional, non-deterministic, then persistent" character the user describes.
- **`webrtcbin latency` is fed from `--jitterBufferLatency`, which defaults to 0**
  (`Config.h:22`, `Pipeline.cpp:163-172`). That is the *send-side* configuration of the RTP
  jitterbuffer latency; 0 is the most aggressive setting and leaves the least room to absorb
  branch-to-branch scheduling jitter. See "Knob-by-knob" below for why this is the highest-value
  knob to raise first.
- **Transcode vs. bypass is the axis to test first.** The bypass paths
  (`--bypass-video` / `--bypass-audio`, `Pipeline.cpp:451-461`, `Pipeline.cpp:630-640`) skip the
  decode+encode elements entirely, so they carry the demuxer's original PTS through untouched and
  add almost no branch latency. If the drift **disappears on the bypass path and only appears
  when transcoding**, that localises it to decode/encode latency asymmetry (the most likely
  cause) rather than the SRT/tsdemux front end. This correlation is the single most useful data
  point to capture in the live repro.

---

## 1. Pipeline topology and where the two branches diverge

`tsdemux` fires a pad per elementary stream; `onDemuxPadAdded` (`Pipeline.cpp:385-421`) routes
each to a codec-specific linker. For the reported input (H264 video + AAC audio) the two live
branches are:

**Video, transcode (default):** `onH264SinkPadAdded` else-branch, `Pipeline.cpp:462-484`
```
tsdemux → h264parse → avdec_h264 → [clockoverlay?] → videoconvert →
         x264enc → capsfilter → rtph264pay → queue → webrtcbin
```
**Video, bypass (`--bypass-video`):** `Pipeline.cpp:451-461`
```
tsdemux → h264parse → rtph264pay → queue → webrtcbin
```
**Audio, transcode (default):** `onAacSinkPadAdded`, `Pipeline.cpp:571-582`
```
tsdemux → aacparse → avdec_aac → audioconvert → audioresample →
         opusenc → rtpopuspay → queue → webrtcbin
```

The two default branches are highly asymmetric: video runs a full H264 decode + `videoconvert`
+ `x264enc` (`threads=2`, `Pipeline.cpp:93-104`), audio runs an AAC decode + `opusenc`. These do
not add equal latency, and nothing downstream re-aligns them — `webrtcbin` timestamps RTP from
each branch's buffer PTS independently. Any fixed latency difference between the branches shows
up at the receiver as a constant lip-sync offset; audio being the *shorter/faster* branch is
consistent with the reported **audio-ahead-in-time / "video behind"** — equivalently audio
appearing to lead, which a viewer perceives as audio-behind-video only if video is the leading
reference. The direction is worth confirming precisely in repro (see procedure), because it
tells you which branch to add compensating latency to.

## 2. Timestamp preservation across the decode/re-encode boundary

- **tsdemux** derives PTS/DTS from the PES headers and, unless `--ignore-pcr` is set, reclocks
  against the PCR. `ignore-pcr` is wired at `Pipeline.cpp:357` from `Config.ignorePcr_`
  (default false, `Config.h:32`; also settable via the `IGNORE_PCR` env var, `Config.h:36-41`).
  With PCR honoured, tsdemux output carries correct, PCR-aligned PTS for **both** streams — this
  is the common time base the rest of the pipeline relies on for sync.
- **Parsers** (`h264parse`, `aacparse`) forward PTS/DTS; `h264parse` gets
  `disable-passthrough=TRUE` on the transcode path only (`Pipeline.cpp:73-76`) so it fully
  re-frames, but that does not drop timestamps.
- **Decoders/encoders** (`avdec_h264`/`avdec_aac`, `x264enc`/`opusenc`) re-derive output PTS from
  input PTS in the normal GStreamer way. `x264enc tune=zerolatency` (`Pipeline.cpp:99`) disables
  B-frames/lookahead, which keeps DTS==PTS and minimises the video encoder's own added latency —
  good for this problem, and means the residual video-branch latency is dominated by decode +
  `videoconvert` + encode pipelining, not reordering.
- **No manual timestamp code exists** on any branch, so there is no branch that *loses* PTS.

Conclusion: the timestamps themselves are preserved; what differs is the *wall-clock arrival
time* of equally-timestamped audio vs. video buffers at `webrtcbin`, governed by branch latency
and queue behaviour — i.e. a tuning problem, addressed below.

## 3. Knob-by-knob: what each latency control actually does here

| Knob | Config / code | Default | Effect on A/V drift |
|------|---------------|---------|---------------------|
| `--jitterBufferLatency` | `Config.h:22`; set as `webrtcbin` **`latency`** at `Pipeline.cpp:171` | **0** | Send-side RTP jitterbuffer latency budget. **0 is the most aggressive** and gives the least slack to absorb inter-branch scheduling jitter. Raising it is the first thing to try. |
| `--tsDemuxLatency` | `Config.h:21`; set as `tsdemux` **`latency`** at `Pipeline.cpp:356` | **0** | tsdemux's PCR/PTS estimation + output latency window (ms). A larger window lets tsdemux emit more stably-timestamped buffers on both pads; too small can make PTS estimation jittery on marginal SRT input. |
| `--srtSourceLatency` | `Config.h:23`; set as `srtsrc`/`srtsink` **`latency`** at `Pipeline.cpp:262`, `280`, `335` | **125** (ms) | SRT receive buffer / ARQ recovery window. Too low → retransmit-induced gaps and packet reordering reach tsdemux unevenly, which can perturb one PES stream more than the other and *cause* transient drift. Should be ≥ the path RTT. |
| `-d/--udpSourceQueueMinTime` | `Config.h`; set as `UDP_QUEUE` **`min-threshold-time`** (ns) at `Pipeline.cpp:361-364` | **0** | Pre-demux hold-back on the *combined* TS before demux. Applies before the streams split, so it delays both equally and does **not** by itself fix inter-branch drift; useful only to smooth bursty SRT delivery into tsdemux. |
| (implicit) payload `queue` sizing | `Pipeline.cpp:762-767` | **unbounded** | All `queue`s have `max-size-buffers/bytes/time = 0`. Unbounded queues cannot back-pressure or drop, so a transient branch stall becomes a *persistent* offset instead of self-correcting. This is the structural amplifier behind "occasional → then it stays offset". |

## 4. Drift hypotheses mapped to code

1. **Transcode-latency asymmetry (most likely).** Video transcode branch
   (`Pipeline.cpp:473-483`) is inherently higher-latency than the audio branch
   (`Pipeline.cpp:571-578`); nothing re-syncs them. → Test bypass vs. transcode; if drift is
   transcode-only, this is it.
2. **SRT recovery jitter perturbing one PES stream.** With `--srtSourceLatency` default 125 ms
   (`Config.h:23`), on a lossy/high-RTT link ARQ gaps reach `tsdemux` unevenly. → Raise
   `--srtSourceLatency` above path RTT and see if the *onset* of drift stops.
3. **Zero jitterbuffer slack.** `webrtcbin latency=0` (`Pipeline.cpp:171`,
   default `Config.h:22`) leaves no room to absorb inter-branch scheduling jitter. → Raise
   `--jitterBufferLatency`.
4. **Unbounded queues freezing a transient offset.** `Pipeline.cpp:762-767`. Any one-off stall
   is absorbed permanently. → This is structural; note it, but do not change queue sizing as part
   of a triage spike (it changes behaviour for every path and needs its own live validation).

## 5. Recommended latency settings (starting point to validate in repro)

For an SRT-ingested mpegts/H264+AAC stream transcoded to H264/OPUS over WHIP, start here and
adjust against a measured lip-sync offset:

- `--srtSourceLatency 200` (or ≥ 3-4x the measured path RTT; the 125 ms default is often too low
  for anything but a LAN and is a plausible drift-*onset* trigger).
- `--jitterBufferLatency 100` to `200` — give `webrtcbin` a non-zero jitterbuffer budget instead
  of the default 0 so brief inter-branch scheduling jitter is absorbed rather than shipped as
  offset.
- `--tsDemuxLatency 100` — a modest, non-zero tsdemux window for steadier PCR/PTS estimation on
  both pads instead of the default 0.
- Leave `-d/--udpSourceQueueMinTime` at 0 unless SRT delivery into tsdemux is visibly bursty; it
  delays both streams equally and does not correct inter-branch drift.
- **Diagnostic A/B:** run once with `--bypass-video --bypass-audio` (requires H264+OPUS input —
  note the reported source is AAC, so full audio bypass needs an OPUS source or applies to video
  only) and once fully transcoding. If drift is present only when transcoding, the fix belongs at
  the decode/encode branch-latency layer, not in the SRT/tsdemux knobs.

These are conservative starting values, not a proven fix — they must be checked against a live
lip-sync measurement (below) before being promoted into README guidance or defaults.

## 6. Live reproduction procedure (to run before any fix)

1. Generate a controlled SRT mpegts source with a known-good hard sync reference (e.g. a
   clapper / timecode-burn test pattern with a matching audio beep), muxed as mpegts +
   libx264 video + AAC audio, pushed over SRT to the port `whip-mpegts` listens on.
2. Run `whip-mpegts -s` (SRT transport) against a real WHIP endpoint, first with defaults, then
   with the settings in §5, then with `--bypass-video`/`--bypass-audio`.
3. At the WebRTC receiver, measure the audio-vs-video offset off the burned-in reference and
   record: transcode-vs-bypass, `--srtSourceLatency`, `--jitterBufferLatency`, `--tsDemuxLatency`,
   whether the offset is constant or grows, and link RTT/loss.
4. Capture GStreamer timing with `GST_DEBUG=tsdemux:5,*jitterbuffer*:5` and dump the pipeline
   graph via the existing `SIGHUP` → `.dot` hook (`Pipeline.cpp:66-71`, `908-915`;
   set `GST_DEBUG_DUMP_DOT_DIR`) to confirm the branch each stream takes.
5. Only once the transcode-vs-bypass correlation and a repeatable offset are established, scope
   the actual fix (compensating branch latency, bounded/leaky payload queues, or a non-zero
   default for one of the knobs) as a follow-up issue.

## Open questions / follow-ups

1. **Confirm drift direction and transcode correlation** (blocking any fix) — the §6 A/B is the
   gate.
2. **Should the payload `queue`s be bounded/leaky?** `Pipeline.cpp:762-767` makes every queue
   unbounded; a bounded/`leaky=downstream` payload queue would let a transient offset self-correct
   instead of persisting — but this changes behaviour on *every* path and must be validated live,
   so it is a separate follow-up, not part of this triage.
3. **Non-zero defaults for `--jitterBufferLatency` / `--tsDemuxLatency`?** Both default to 0; if
   the §5 values prove out in repro, consider raising the defaults or documenting them as
   recommended in the README. Do not change defaults on the strength of static analysis alone.
