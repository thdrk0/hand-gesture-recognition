"""Webcam demo: python app.py --camera 0"""
import argparse
import json
import sys
import time
import urllib.request
from pathlib import Path

from gestures import Stabilizer, finger_states

MODEL_URL = "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task"
CONNECTIONS = [(0, 1), (1, 2), (2, 3), (3, 4), (0, 5), (5, 6), (6, 7), (7, 8),
               (5, 9), (9, 10), (10, 11), (11, 12), (9, 13), (13, 14), (14, 15),
               (15, 16), (13, 17), (0, 17), (17, 18), (18, 19), (19, 20)]


def ensure_model(path):
    if path.is_file():
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    print("Telechargement du modele MediaPipe (premier lancement)...", flush=True)
    temporary = path.with_suffix(".download")
    try:
        urllib.request.urlretrieve(MODEL_URL, temporary)
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description="Reconnaissance des chiffres 1 a 5 avec la webcam")
    parser.add_argument("--camera", type=int, default=0, help="Index de webcam (defaut: 0)")
    parser.add_argument("--hold", type=float, default=0.45, help="Duree de stabilisation en secondes")
    parser.add_argument("--model", type=Path, default=Path(__file__).parent / "models/hand_landmarker.task")
    parser.add_argument("--events", type=Path, help="Journal optionnel des actions au format JSONL")
    args = parser.parse_args()
    if args.hold <= 0:
        parser.error("--hold doit etre positif")
    import cv2
    import mediapipe as mp
    ensure_model(args.model)
    options = mp.tasks.vision.HandLandmarkerOptions(
        base_options=mp.tasks.BaseOptions(model_asset_path=str(args.model)),
        running_mode=mp.tasks.vision.RunningMode.VIDEO,
        num_hands=1,
        min_hand_detection_confidence=0.6,
        min_hand_presence_confidence=0.6,
        min_tracking_confidence=0.6,
    )
    camera = cv2.VideoCapture(args.camera)
    log = None
    try:
        if not camera.isOpened():
            raise RuntimeError("Webcam inaccessible. Essayez --camera 1 et verifiez les permissions camera. Sous WSL, lancez plutot ce programme avec Python Windows.")
        if args.events:
            args.events.parent.mkdir(parents=True, exist_ok=True)
            log = args.events.open("a", encoding="utf-8")
        stable = Stabilizer(args.hold)
        selected = None
        previous = time.monotonic()
        timestamp = -1
        fps = 0.0
        with mp.tasks.vision.HandLandmarker.create_from_options(options) as detector:
            while True:
                ok, frame = camera.read()
                if not ok:
                    raise RuntimeError("Lecture webcam interrompue.")
                frame = cv2.flip(frame, 1)
                now = time.monotonic()
                timestamp = max(timestamp + 1, int(now * 1000))
                rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                result = detector.detect_for_video(mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb), timestamp)
                raw = None
                states = None
                if result.hand_world_landmarks:
                    points = [(p.x, p.y, p.z) for p in result.hand_world_landmarks[0]]
                    states = finger_states(points)
                    count = sum(states)
                    raw = count if 1 <= count <= 5 else None
                    height, width = frame.shape[:2]
                    pixels = [(int(p.x * width), int(p.y * height)) for p in result.hand_landmarks[0]]
                    for a, b in CONNECTIONS:
                        cv2.line(frame, pixels[a], pixels[b], (70, 230, 140), 2)
                    for point in pixels:
                        cv2.circle(frame, point, 4, (255, 220, 70), -1)
                current, event = stable.update(raw, now)
                if event is not None:
                    selected = event
                    record = {"time": time.time(), "gesture": event, "action": f"selection_{event}"}
                    print(json.dumps(record), flush=True)
                    if log:
                        log.write(json.dumps(record) + "\n")
                        log.flush()
                fps = 0.9 * fps + 0.1 / max(now - previous, 1e-6)
                previous = now
                cv2.rectangle(frame, (0, 0), (frame.shape[1], 125), (25, 28, 35), -1)
                visible = current if raw == current else None
                cv2.putText(frame, f"Chiffre : {visible if visible else '-'}   FPS : {fps:.0f}", (15, 32), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (90, 240, 160), 2)
                cv2.putText(frame, "Montrez 1 a 5 doigts | Q / Echap : quitter", (15, 65), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (240, 240, 240), 1)
                cv2.putText(frame, f"Derniere action : selection {selected if selected else '-'}", (15, 94), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 220, 100), 1)
                diagnostic = "Main absente"
                if states is not None:
                    diagnostic = "  ".join(f"{name}:{int(up)}" for name, up in zip(("Pouce", "Index", "Majeur", "Annul", "Auric"), states))
                cv2.putText(frame, diagnostic, (15, 118), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (240, 240, 240), 1)
                cv2.imshow("Hand Gesture - Webcam", frame)
                if cv2.waitKey(1) & 0xFF in (ord("q"), ord("Q"), 27):
                    break
                if cv2.getWindowProperty("Hand Gesture - Webcam", cv2.WND_PROP_VISIBLE) < 1:
                    break
    finally:
        camera.release()
        cv2.destroyAllWindows()
        if log:
            log.close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
    except (RuntimeError, OSError, ValueError, ImportError) as error:
        print(f"Erreur : {error}", file=sys.stderr)
        sys.exit(1)
