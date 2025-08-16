# **TETHER The ESP32 ChatGPT TextBot**

*“Using AI as your overkill, but hilarious, SMS Assistant.”*

---

## About This Madness

Ever wondered what it’s like to interrogate ChatGPT from your ancient flip phone? 
Me neither, I was just sick of using data to look up random facts on hikes.
but here we are.

This project:
- Receives SMS messages via a GSM module.
- Forwards them to ChatGPT over Wi‑Fi.
- Attempts to trim the AI’s response to 160 characters—because SMS hates you.
- Sends the reply right back to your phone via SMS.
- **Remembers conversations per number** for 5 minutes (then ghosts you).
- Has **OTA updates** so your sealed-in-a-box device stays updatable.

---

## What You’ll Need

- **ESP32** — way more brainpower than necessary, but allows for improvements.
- **A7670SA GSM module** — This may vary based on your region.
- **SIM card with credit** — can’t SMS with good vibes only.
- **5 V power source** - power decoys are cheap and easy.
- **Wi‑Fi access** — because ChatGPT doesn’t answer via Morse code.
- **Arduino IDE** — where your code comes alive.

---

## Wiring Diagram (MS Paint-Ready)

```

```

---

## How It Works (Barely)

1. SMS arrives.
2. ESP32 reads sender number and message.
3. chaperones the message to ChatGPT.
4. Gets an answer, chops it at 160 chars, texts it back to you.
5. Deletes the SMS so the cycle doesn’t loop into oblivion.
6. Keeps context per sender for 5 minutes before wiping memory. Simple but effective.

---

## OTA Magic

You don’t need to crack open the box again as the OS updates itself over Wi‑Fi:

```cpp
ArduinoOTA.begin();
// in loop:
ArduinoOTA.handle();
```

Silent firmware ninja-updates.

---

## Future Shenanigans

- Make it send passive-aggressive responses if your grammar sucks.
- Add voice calls so it reads replies like a robo-pal.
- Include admin commands—text “RESET” and it flips the script.
- Maybe even get it to send daily quests!

---

## Want to Use It?

1. Clone this madness.
2. Install dependencies (`ArduinoJson`, `ArduinoOTA`).
3. Set your Wi-Fi & API key in `Config.h`.
4. Upload via serial (first time only).
5. Seal it in a box and forget it.
6. Get a text 12 years later asking why you forgot about it.

---

### TL;DR

Texting you can add to your projects, the assistant is secondary.
