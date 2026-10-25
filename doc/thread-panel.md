# Thread and pool panel

`/debug threads` toggles the centered panel. `/debug threads next` and
`/debug threads previous` change pages. The panel is independent of the
legacy debug HUD. Hiding the whole interface pauses its collection. All UI
labels are in English.

Each card has two bars on the left and aligned text on the right. Busy time
is the measured work duration in the last half-second divided by the available
worker time; the other bar is queued work relative to that pool's observed
peak. Dedicated threads have no engine task queue and say **No queue**.
The cards are 84 pixels apart and only as many as fit the screen are drawn on
each page. Names and long values are shortened on narrow screens.

The local world card uses the existing world performance snapshot. Its busy
percentage is wall-clock tick activity, and its queue is the visible sector
queue. It only reflects the host's world thread in local play; a remote client
does not receive server thread statistics.

Generic `WorkerPool` instances report worker count, active workers, queued and
peak tasks, submitted and completed jobs, and accumulated work duration. This
includes sector generation, HTTP and sector array pools when instantiated.
The queue counter starts at zero when collection is enabled and reflects the
next enqueue or dequeue; tasks already waiting at activation are not counted
until the queue changes.
Dedicated measurement points cover world server ticks, lighting, voice encoding,
network workers, TCP accept callbacks, universe server work, root maintenance,
scriptable Lua threads, client subworld threads and system world server threads.
The current local world also has a separate detailed card with tick and sector
queue information; it refers to the same server thread as its dedicated row.
Dedicated rows measure explicit work sections, excluding sleep intervals. They
appear after their first measured call and disappear after 30 seconds without a
new sample. The registry limits dedicated names to 64 and evicts names idle
for five minutes. OS, driver and external library threads are outside this
instrumented engine view.

Monitoring defaults to off and is enabled only while the panel is visible.
With it off, worker tasks only check the enable flag; no task clocks,
per-task counters, histograms or UI snapshots are recorded. Once on, pool
counters use atomics and dedicated thread samples use a small registry lock.
Snapshots are read at most twice per second. Reported busy time is wall time,
including any waits inside measured work; it is not per-core CPU utilization.

This change does not move work between threads, alter world simulation or add
network packets. Windows gameplay and the visual layout still require a user
build and inspection. GitHub Actions remains manual.
