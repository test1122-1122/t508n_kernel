#!/usr/bin/env bash
# Detached restart of the local DSH web GUI (127.0.0.1:3080) so a profile-patch
# change (model maxTokens, minimal-preset compaction group) takes effect.
#
# Usage: setsid nohup bash dsh-web-restart.sh >/tmp/dsh_web_restart.log 2>&1 &
#
# It runs detached from the caller on purpose: the caller is usually a turn
# inside that very web server, so the old server's death must not take the
# restart with it. Every step is logged with a timestamp.
set -u
exec >>/tmp/dsh_web_lifecycle.log 2>&1
log() { printf '%s %s\n' "$(date -Iseconds)" "$*"; }

CHECKOUT=/home/test/.npm/_npx/1e7f6d9597241db0
WORKDIR=/home/test/t508n
LOG=/tmp/dsh_web.log
PORT=3080

log "restart: begin (old listeners: $(ss -ltnp 2>/dev/null | grep -c ":$PORT ") )"

# 1. Stop the old server: the launcher is `npm exec @deepseek-ai/dsh web`, which
#    is its own process group, so one negative-PID signal takes all three down.
old=$(ss -ltnp 2>/dev/null | grep ":$PORT " | sed -n 's/.*pid=\([0-9]*\).*/\1/p' | head -1)
if [ -n "${old:-}" ]; then
  pgid=$(ps -o pgid= -p "$old" | tr -d ' ')
  log "restart: stopping pid=$old pgid=$pgid"
  kill -TERM "-$pgid" 2>/dev/null || kill -TERM "$old" 2>/dev/null
  for _ in $(seq 1 60); do
    ss -ltn 2>/dev/null | grep -q ":$PORT " || break
    sleep 0.5
  done
  if ss -ltn 2>/dev/null | grep -q ":$PORT "; then
    log "restart: port still busy after 30s, sending SIGKILL"
    kill -KILL "-$pgid" 2>/dev/null || kill -KILL "$old" 2>/dev/null
    sleep 2
  fi
else
  log "restart: no listener on $PORT found (starting fresh)"
fi

# 2. Start the new server fully detached, same launch shape as before.
cd "$WORKDIR" || exit 1
: >"$LOG"
setsid nohup npm exec @deepseek-ai/dsh web >>"$LOG" 2>&1 &
newjob=$!
log "restart: launched detached pid=$newjob, log=$LOG"

# 3. Wait for the port to answer again.
for _ in $(seq 1 120); do
  if ss -ltn 2>/dev/null | grep -q ":$PORT " && curl -s -o /dev/null -m 2 "http://127.0.0.1:$PORT/"; then
    log "restart: OK - listening again on http://127.0.0.1:$PORT"
    ss -ltnp 2>/dev/null | grep ":$PORT " | sed 's/^/restart: /'
    exit 0
  fi
  sleep 0.5
done

log "restart: FAILED - port did not answer within 60s; tail of $LOG follows"
tail -n 40 "$LOG"
exit 1
