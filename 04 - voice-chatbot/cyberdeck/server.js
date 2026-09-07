require("dotenv").config();

const express = require("express");
const cors = require("cors");
const { GoogleGenAI } = require("@google/genai");

const app = express();

const PORT = process.env.PORT || 3000;
const GEMINI_API_KEY = process.env.GEMINI_API_KEY;
const DEEPGRAM_API_KEY = process.env.DEEPGRAM_API_KEY;

const CHAT_MODEL = process.env.GEMINI_MODEL || "gemini-3.6-flash";
const DEEPGRAM_STT_MODEL = process.env.DEEPGRAM_STT_MODEL || "nova-3";
const DEEPGRAM_TTS_MODEL =
  process.env.DEEPGRAM_TTS_MODEL || "aura-2-thalia-en";

if (!GEMINI_API_KEY) {
  console.error("ERROR: GEMINI_API_KEY is missing in .env");
  process.exit(1);
}

if (!DEEPGRAM_API_KEY) {
  console.error("ERROR: DEEPGRAM_API_KEY is missing in .env");
  process.exit(1);
}

const ai = new GoogleGenAI({ apiKey: GEMINI_API_KEY });

app.use(cors());
app.use(express.json({ limit: "64kb" }));

async function deepgramTranscribe(wavBuffer) {
  const url = new URL("https://api.deepgram.com/v1/listen");

  url.searchParams.set("model", DEEPGRAM_STT_MODEL);
  url.searchParams.set("smart_format", "true");
  url.searchParams.set("punctuate", "true");
  url.searchParams.set("language", "en");

  console.log(`Uploading ${wavBuffer.length} bytes to Deepgram STT...`);

  const response = await fetch(url, {
    method: "POST",
    headers: {
      Authorization: `Token ${DEEPGRAM_API_KEY}`,
      "Content-Type": "audio/wav",
      "Content-Length": String(wavBuffer.length),
      "Connection": "close"
    },
    body: wavBuffer
  });

  const responseText = await response.text();

  console.log(`Deepgram STT status: ${response.status}`);

  if (!response.ok) {
    throw new Error(
      `Deepgram STT HTTP ${response.status}: ${responseText}`
    );
  }

  const json = JSON.parse(responseText);

  return String(
    json?.results?.channels?.[0]?.alternatives?.[0]?.transcript || ""
  ).trim();
}

async function askNebula(question) {
  const response = await ai.models.generateContent({
    model: CHAT_MODEL,
    contents: `
You are Nebula, a friendly CyberDeck assistant at a beginner electronics workshop.
Answer clearly and safely in plain English.
Keep the answer under 120 characters and at most two short sentences.
Do not use Markdown, lists, emojis, or special formatting.

User question: ${question}
`
  });

  return String(response.text || "").trim();
}

async function deepgramTtsWav(text) {
  const url = new URL("https://api.deepgram.com/v1/speak");

  url.searchParams.set("model", DEEPGRAM_TTS_MODEL);
  url.searchParams.set("encoding", "linear16");
  url.searchParams.set("container", "wav");
  url.searchParams.set("sample_rate", "24000");

  const response = await fetch(url, {
    method: "POST",
    headers: {
      Authorization: `Token ${DEEPGRAM_API_KEY}`,
      "Content-Type": "application/json"
    },
    body: JSON.stringify({ text })
  });

  if (!response.ok) {
    const errorText = await response.text();

    throw new Error(
      `Deepgram TTS HTTP ${response.status}: ${errorText}`
    );
  }

  const wavBuffer = Buffer.from(await response.arrayBuffer());

  if (wavBuffer.length < 44) {
    throw new Error("Deepgram TTS returned too little audio.");
  }

  if (wavBuffer.toString("ascii", 0, 4) !== "RIFF") {
    throw new Error("Deepgram TTS did not return a WAV file.");
  }

  console.log(`TTS generated: ${wavBuffer.length} WAV bytes`);
  return wavBuffer;
}

app.get("/", (req, res) => {
  res.json({
    status: "Nebula Deepgram + Gemini voice server is running",
    endpoints: {
      health: "GET /health",
      ask: "POST /ask with JSON { question }",
      transcribe: "POST /transcribe with audio/wav body",
      tts: "POST /tts with JSON { text }; returns audio/wav"
    }
  });
});

app.get("/health", (req, res) => {
  res.json({
    ok: true,
    service: "nebula-deepgram-gemini",
    chatModel: CHAT_MODEL,
    deepgramSttModel: DEEPGRAM_STT_MODEL,
    deepgramTtsModel: DEEPGRAM_TTS_MODEL,
    audioOutput: "WAV / 16-bit PCM / mono / 24000 Hz"
  });
});

app.post("/ask", async (req, res) => {
  try {
    const question = String(req.body?.question || "").trim();

    if (!question) {
      return res.status(400).json({
        ok: false,
        error: "Missing question"
      });
    }

    if (question.length > 400) {
      return res.status(400).json({
        ok: false,
        error: "Question is too long; maximum is 400 characters."
      });
    }

    console.log(`Question: ${question}`);

    const reply = await askNebula(question);

    if (!reply) {
      return res.status(502).json({
        ok: false,
        error: "Gemini returned an empty reply."
      });
    }

    console.log(`Reply: ${reply}`);
    res.json({ ok: true, reply });
  } catch (error) {
    console.error("Chat request failed:", error);

    res.status(500).json({
      ok: false,
      error: "Chatbot request failed.",
      detail: error.message || "Unknown error"
    });
  }
});

app.post(
  "/transcribe",
  express.raw({ type: "audio/wav", limit: "512kb" }),
  async (req, res) => {
    try {
      if (!Buffer.isBuffer(req.body) || req.body.length < 100) {
        return res.status(400).json({
          ok: false,
          error: "Send a WAV body with Content-Type: audio/wav."
        });
      }

      console.log(`Voice upload received: ${req.body.length} bytes`);

      const transcript = await deepgramTranscribe(req.body);

      if (!transcript) {
        return res.status(502).json({
          ok: false,
          error: "Deepgram returned an empty transcript."
        });
      }

      console.log(`Transcript: ${transcript}`);

      res.json({ ok: true, transcript });
    } catch (error) {
      console.error("Transcription failed:", error);

      res.status(500).json({
        ok: false,
        error: "Transcription failed.",
        detail: error.message || "Unknown error"
      });
    }
  }
);

app.post("/tts", async (req, res) => {
  try {
    const text = String(req.body?.text || "").trim();

    if (!text) {
      return res.status(400).json({
        ok: false,
        error: "Missing text"
      });
    }

    if (text.length > 240) {
      return res.status(400).json({
        ok: false,
        error: "Text is too long; maximum is 240 characters."
      });
    }

    console.log(`TTS request: ${text}`);

    const wavBuffer = await deepgramTtsWav(text);

    res.setHeader("Content-Type", "audio/wav");
    res.setHeader("Content-Length", String(wavBuffer.length));
    res.setHeader("Cache-Control", "no-store");
    res.send(wavBuffer);
  } catch (error) {
    console.error("TTS failed:", error);

    res.status(500).json({
      ok: false,
      error: "TTS request failed.",
      detail: error.message || "Unknown error"
    });
  }
});

app.listen(PORT, "0.0.0.0", () => {
  console.log(`Nebula server running on http://0.0.0.0:${PORT}`);
  console.log(`Health: http://localhost:${PORT}/health`);
  console.log(`Transcribe: POST /transcribe`);
  console.log(`Ask: POST /ask`);
  console.log(`TTS: POST /tts`);
});
