"""Local MusicVAE HTTP server for the DrumSpace plugin (drums model).

Same JSON-over-HTTP protocol as the MidiSpace melody server, but serves the
Magenta `cat-drums_2bar_small.hikl` checkpoint (2-bar drums, 9 classes, z=256).

  POST /encode  {"notes": [[pitch, start_quarter, end_quarter], ...]}  -> {"z": [256 floats]}
  POST /decode  {"z": [256 floats], "temperature": 0.3}               -> {"notes": [[...], ...]}
  GET  /health  -> {"status": "ok"}

Drum notes use General MIDI drum pitches (kick=36, snare=38, closed HH=42,
open HH=46, low tom=45, mid tom=48, high tom=50, crash=49, ride=51).  Unlike
melody, several drums may sound on the same step (polyphonic).
"""
import json
import os
import sys
import threading

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

# Locate the model checkpoints directory.  When frozen by PyInstaller,
# resolve relative to the executable so the model can sit next to the exe
# (exe_dir/checkpoints/...); when run from source, use the repo layout.
def _base_dir():
    if getattr(sys, 'frozen', False):
        return os.path.dirname(os.path.abspath(sys.executable))
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')

CHECKPOINTS = os.path.join(_base_dir(), 'checkpoints')

MODEL = None
LOCK = threading.Lock()

CONFIG_NAME = 'cat-drums_2bar_small'   # config id; '.hikl' is just the ckpt suffix
CHECKPOINT_PREFIX = os.path.join(
    CHECKPOINTS, 'cat-drums_2bar_small.hikl.ckpt')

# 9 drum classes (exemplar pitch per class), matching magenta's
# DrumsConverter REDUCED_DRUM_PITCH_CLASSES.
DRUM_PITCHES = [36, 38, 42, 46, 45, 48, 50, 49, 51]


def get_model():
    global MODEL
    if MODEL is None:
        print('loading drum model from', CHECKPOINT_PREFIX, flush=True)
        from magenta.models.music_vae import TrainedModel, configs
        config = configs.CONFIG_MAP[CONFIG_NAME]
        MODEL = TrainedModel(config, batch_size=4,
                             checkpoint_dir_or_path=CHECKPOINT_PREFIX)
        print('drum model loaded', flush=True)
    return MODEL


def notes_to_note_sequence(notes):
    """Build a 2-bar drum NoteSequence from (pitch, start_quarter, end_quarter).

    Drum notes are polyphonic and marked is_drum=True.  Times are in absolute
    seconds (magenta convention): at qpm=120 one quarter = 0.5 s, so 2 bars =
    8 quarters = 4.0 s.
    """
    from magenta.protobuf import music_pb2
    ns = music_pb2.NoteSequence()
    ns.ticks_per_quarter = 220
    ts = ns.time_signatures.add()
    ts.time = 0.0
    ts.numerator = 4
    ts.denominator = 4
    tempo = ns.tempos.add()
    tempo.time = 0.0
    tempo.qpm = 120.0
    spq = 0.5
    total = 0.0
    for pitch, sq, eq in notes:
        n = ns.notes.add()
        n.pitch = int(pitch)
        n.velocity = 100
        n.start_time = sq * spq
        n.end_time = eq * spq
        n.is_drum = True
        total = max(total, eq * spq)
    ns.total_time = total
    return ns


def note_sequence_to_notes(ns):
    """Flatten a drum NoteSequence to (pitch, start_quarter, end_quarter) events.

    Polyphonic: every drum note is returned (same-step hits are preserved).
    """
    qpm = ns.tempos[0].qpm if ns.tempos else 120.0
    spq = 60.0 / qpm
    events = []
    for note in ns.notes:
        if not note.is_drum:
            continue
        events.append([
            int(note.pitch),
            round(note.start_time / spq, 4),
            round(note.end_time / spq, 4),
        ])
    return events


class Handler(BaseHTTPRequestHandler):
    def _send(self, obj, code=200):
        body = json.dumps(obj).encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == '/health':
            self._send({'status': 'ok'})
        else:
            self._send({'error': 'not found'}, 404)

    def do_POST(self):
        try:
            length = int(self.headers.get('Content-Length', 0))
            req = json.loads(self.rfile.read(length).decode('utf-8'))
        except Exception as e:
            self._send({'error': 'bad request: %s' % e}, 400)
            return

        model = get_model()
        try:
            if self.path == '/encode':
                with LOCK:
                    # encode() -> (z, mu, sigma); use mu (posterior mean) for
                    # deterministic interpolation, matching the melody server.
                    mu = model.encode([notes_to_note_sequence(req['notes'])])[1]
                self._send({'z': mu[0].tolist()})
            elif self.path == '/decode':
                import numpy as np
                z = np.asarray(req['z'], dtype=np.float32)[None, :]
                temp = float(req.get('temperature', 0.3))
                with LOCK:
                    ns = model.decode(z, length=32, temperature=temp)[0]
                self._send({'notes': note_sequence_to_notes(ns)})
            else:
                self._send({'error': 'not found'}, 404)
        except Exception as e:
            self._send({'error': repr(e)}, 500)

    def log_message(self, *args):
        pass


def main():
    port = int(os.environ.get('DRUMSPACE_PORT', '8766'))
    get_model()
    print('DrumSpace server listening on 127.0.0.1:%d' % port, flush=True)
    ThreadingHTTPServer(('127.0.0.1', port), Handler).serve_forever()


if __name__ == '__main__':
    main()
