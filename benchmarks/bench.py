#!/usr/bin/env python3
"""End-to-end Tizcord channel benchmark; run on Linux after `make -C server`."""
import argparse
import asyncio
import ctypes as C
import json
import os
from pathlib import Path
import platform
import socket
import sqlite3
import struct
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]

class Auth(C.Structure):
    _fields_ = [('action', C.c_int), ('status', C.c_int), ('username', C.c_char * 32), ('password', C.c_char * 64)]
class Channel(C.Structure):
    _fields_ = [('action', C.c_int), ('server_id', C.c_int64), ('status', C.c_int), ('channel_id', C.c_int64), ('name', C.c_char * 32), ('message_id', C.c_int64), ('message', C.c_char * 512)]
class Payload(C.Union):
    _fields_ = [('auth', Auth), ('channel', Channel)]
class Packet(C.Structure):
    _fields_ = [('type', C.c_int), ('sender_id', C.c_int64), ('list_id', C.c_int32), ('list_index', C.c_int32), ('list_total', C.c_int32), ('list_frame', C.c_int), ('payload', Payload), ('timestamp', C.c_int64)]

SIZE = C.sizeof(Packet)
BASE = Packet.payload.offset

def put32(buf, offset, value): struct.pack_into('!i', buf, offset, value)
def get32(buf, offset): return struct.unpack_from('!i', buf, offset)[0]
def put64(buf, offset, value): struct.pack_into('!q', buf, offset, value)

def auth_packet(name, action=1):
    b = bytearray(SIZE)
    put32(b, Packet.type.offset, 0)
    put32(b, BASE + Auth.action.offset, action)
    b[BASE + Auth.username.offset:BASE + Auth.username.offset + len(name)] = name.encode()
    b[BASE + Auth.password.offset:BASE + Auth.password.offset + 8] = b'benchpw!'
    return b

def channel_packet(action, message=''):
    b = bytearray(SIZE)
    put32(b, Packet.type.offset, 3)
    put32(b, BASE + Channel.action.offset, action)
    put64(b, BASE + Channel.server_id.offset, 1)
    put64(b, BASE + Channel.channel_id.offset, 1)
    raw = message.encode()
    if len(raw) >= 512: raise ValueError('message too long')
    b[BASE + Channel.message.offset:BASE + Channel.message.offset + len(raw)] = raw
    return b

def percentile(values, p):
    values = sorted(values)
    return round(values[min(len(values)-1, int((len(values)-1)*p))] * 1000, 3)

def proc_stats(pid):
    with open(f'/proc/{pid}/stat') as f: fields = f.read().rsplit(')', 1)[1].split()
    with open(f'/proc/{pid}/status') as f: status = f.read()
    rss = next(int(x.split()[1]) for x in status.splitlines() if x.startswith('VmHWM:'))
    return int(fields[11]) + int(fields[12]), rss

class Client:
    def __init__(self, reader, writer):
        self.reader, self.writer = reader, writer
        self.auth = asyncio.Future()
        self.pending = {}
        self.history = asyncio.Queue()
        self.error = None
        self.task = asyncio.create_task(self.read())
    async def read(self):
        try:
            while True:
                b = await self.reader.readexactly(SIZE)
                kind = get32(b, Packet.type.offset)
                if kind == 0 and get32(b, BASE + Auth.action.offset) in (0, 1) and not self.auth.done():
                    self.auth.set_result(get32(b, BASE + Auth.status.offset))
                elif kind == 3:
                    action = get32(b, BASE + Channel.action.offset)
                    frame = get32(b, Packet.list_frame.offset)
                    if action == 2 and frame == 0:
                        raw = b[BASE + Channel.message.offset:BASE + Channel.message.offset + 512]
                        key = raw.split(b'\0', 1)[0].decode(errors='replace')
                        future = self.pending.pop(key, None)
                        if future and not future.done(): future.set_result(time.perf_counter())
                    else:
                        await self.history.put(frame)
        except (asyncio.IncompleteReadError, ConnectionError) as exc:
            self.error = str(exc)
            if not self.auth.done(): self.auth.set_exception(exc)
            for future in self.pending.values():
                if not future.done(): future.set_exception(exc)
    def send(self, packet):
        self.writer.write(packet)
    async def close(self):
        self.writer.close()
        try: await self.writer.wait_closed()
        except ConnectionError: pass
        self.task.cancel()

async def connect(port, name, action=1):
    reader, writer = await asyncio.open_connection('127.0.0.1', port)
    client = Client(reader, writer)
    client.send(auth_packet(name, action))
    await client.writer.drain()
    status = await asyncio.wait_for(client.auth, 30)
    if status != 0: raise RuntimeError(f'registration failed for {name}: {status}')
    return client

async def start_server(binary, port, db, log):
    proc = subprocess.Popen([str(binary), str(port), str(db)], stdout=log, stderr=log)
    for _ in range(100):
        if proc.poll() is not None: raise RuntimeError(f'server exited: {proc.returncode}')
        try:
            reader, writer = await asyncio.open_connection('127.0.0.1', port)
            writer.close(); await writer.wait_closed()
            return proc
        except OSError: await asyncio.sleep(.05)
    raise TimeoutError('server did not start')

async def benchmark(clients, rounds, pid, timeout):
    samples = []
    cpu0, rss0 = proc_stats(pid)
    start = time.perf_counter()
    for round_no in range(rounds):
        futures = []
        for i, client in enumerate(clients):
            key = f'b{round_no:07d}-{i:02d}'
            future = asyncio.get_running_loop().create_future()
            client.pending[key] = future
            sent = time.perf_counter()
            client.send(channel_packet(2, key))
            futures.append((future, sent))
        await asyncio.gather(*(c.writer.drain() for c in clients))
        arrived = await asyncio.wait_for(asyncio.gather(*(f for f, _ in futures)), timeout)
        samples.extend(t - sent for t, (_, sent) in zip(arrived, futures))
    elapsed = time.perf_counter() - start
    cpu1, rss1 = proc_stats(pid)
    ticks = os.sysconf('SC_CLK_TCK')
    return dict(clients=len(clients), messages=len(samples), seconds=round(elapsed, 3),
                messages_per_sec=round(len(samples)/elapsed, 1),
                p50_ms=percentile(samples, .50), p95_ms=percentile(samples, .95),
                p99_ms=percentile(samples, .99), rss_kib=max(rss0, rss1),
                cpu_percent=round(100*(cpu1-cpu0)/ticks/elapsed, 1))

async def stress(port, clients, db, proc, binary, log, timeout):
    results = {}
    # Split one complete packet over many TCP writes.
    writer = None
    try:
        writer = socket.create_connection(('127.0.0.1', port), timeout=timeout)
        packet = channel_packet(2, 'fragmented')
        future = asyncio.get_running_loop().create_future()
        clients[0].pending['fragmented'] = future
        for offset in range(0, len(packet), 17):
            writer.sendall(packet[offset:offset+17])
            await asyncio.sleep(.001)
        await asyncio.wait_for(future, timeout)
        results['partial_writes'] = 'pass'
    except Exception as exc: results['partial_writes'] = f'fail: {type(exc).__name__}'
    finally:
        if writer: writer.close()
    # A partial packet must not stall other clients indefinitely.
    raw = socket.create_connection(('127.0.0.1', port))
    raw.sendall(channel_packet(2, 'partial')[:32])
    try:
        await asyncio.wait_for(benchmark(clients[:1], 1, proc.pid, timeout), timeout)
        results['partial_packet'] = 'pass'
    except Exception as exc: results['partial_packet'] = f'fail: {type(exc).__name__}'
    raw.close()
    # Rapid connection churn, including half-open packets.
    try:
        for _ in range(100):
            s = socket.create_connection(('127.0.0.1', port), timeout=timeout)
            s.sendall(channel_packet(2, 'churn')[:16]); s.close()
        await asyncio.wait_for(benchmark(clients[:1], 1, proc.pid, timeout), timeout)
        results['connect_disconnect'] = 'pass'
    except Exception as exc: results['connect_disconnect'] = f'fail: {type(exc).__name__}'
    # A non-reading peer receives broadcasts while another client makes progress.
    slow = socket.create_connection(('127.0.0.1', port), timeout=timeout)
    slow.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
    try:
        await asyncio.wait_for(benchmark(clients[:1], 100, proc.pid, timeout), timeout)
        results['slow_reader'] = 'pass (100 messages)'
    except Exception as exc: results['slow_reader'] = f'fail: {type(exc).__name__}'
    slow.close()
    # History count includes the benchmark messages and the slow-reader test.
    try:
        expected = sqlite3.connect(db).execute('SELECT COUNT(*) FROM messages WHERE channel_id=1').fetchone()[0]
        clients[0].send(channel_packet(3))
        await clients[0].writer.drain()
        frames = []
        while True:
            frame = await asyncio.wait_for(clients[0].history.get(), timeout)
            frames.append(frame)
            if frame == 3: break
        results['large_history'] = {'expected': expected, 'received': frames.count(2),
                                    'status': 'pass' if frames.count(2) == expected else 'fail'}
    except Exception as exc: results['large_history'] = f'fail: {type(exc).__name__}'
    # Confirm persisted rows survive a server restart.
    before = sqlite3.connect(db).execute('SELECT COUNT(*) FROM messages').fetchone()[0]
    for client in clients: await client.close()
    proc.terminate()
    try: await asyncio.wait_for(asyncio.to_thread(proc.wait), 5)
    except asyncio.TimeoutError: proc.kill(); proc.wait()
    restarted = await start_server(binary, port, db, log)
    after = sqlite3.connect(db).execute('SELECT COUNT(*) FROM messages').fetchone()[0]
    try:
        recovered = await connect(port, 'bench00', action=0)
        recovered.send(channel_packet(3))
        await recovered.writer.drain()
        count = 0
        while True:
            frame = await asyncio.wait_for(recovered.history.get(), timeout)
            if frame == 2: count += 1
            if frame == 3: break
        await recovered.close()
        passed = before == after and count == before
        results['restart'] = {'before': before, 'after': after, 'history_received': count,
                              'status': 'pass' if passed else 'fail'}
    except Exception as exc: results['restart'] = f'fail: {type(exc).__name__}'
    restarted.terminate(); restarted.wait(timeout=5)
    return results

async def main(args):
    binary = Path(args.binary).resolve()
    if not binary.exists(): raise SystemExit('Build the server first: make -C server')
    if not Path('/proc/self/status').exists(): raise SystemExit('Resource sampling requires Linux /proc')
    with tempfile.TemporaryDirectory(prefix='tizcord-bench-') as tmp:
        db = Path(tmp)/'bench.sqlite'
        conn = sqlite3.connect(db)
        conn.executescript((ROOT/'db/schema.sql').read_text())
        conn.commit(); conn.close()
        sock = socket.socket(); sock.bind(('127.0.0.1', 0)); port = sock.getsockname()[1]; sock.close()
        with open(Path(tmp)/'server.log', 'wb') as log:
            proc = await start_server(binary, port, db, log)
            clients = []
            try:
                output = {'host': platform.platform(), 'machine': platform.machine(),
                          'python': platform.python_version(), 'packet_bytes': SIZE,
                          'rounds': args.rounds, 'results': []}
                for count in (1, 8, 32, 64):
                    while len(clients) < count:
                        clients.append(await connect(port, f'bench{len(clients):02d}'))
                    if count == 1:
                        conn = sqlite3.connect(db)
                        conn.execute("INSERT INTO servers(id,name) VALUES(1,'benchmark')")
                        conn.execute("INSERT INTO channels(id,server_id,name) VALUES(1,1,'benchmark')")
                        conn.commit(); conn.close()
                    output['results'].append(await benchmark(clients, args.rounds, proc.pid, args.timeout))
                if args.stress:
                    # Keep one active sender and free slots for stress peers.
                    for client in clients[1:]: await client.close()
                    clients = clients[:1]
                    await asyncio.sleep(.1)
                    output['stress'] = await stress(port, clients, db, proc, binary, log, args.timeout)
                    proc = None
                print(json.dumps(output, indent=2))
            finally:
                for client in clients: await client.close()
                if proc and proc.poll() is None:
                    proc.terminate()
                    try: proc.wait(timeout=5)
                    except subprocess.TimeoutExpired: proc.kill(); proc.wait()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', default=str(ROOT/'server/server'))
    parser.add_argument('--rounds', type=int, default=100)
    parser.add_argument('--timeout', type=float, default=15)
    parser.add_argument('--stress', action='store_true')
    args = parser.parse_args()
    if args.rounds < 1: parser.error('--rounds must be positive')
    asyncio.run(main(args))
