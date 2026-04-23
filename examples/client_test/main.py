from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# Our "database" in memory
base_de_donnees = ["First message"]

# Data model expected from JavaScript
class Message(BaseModel):
    texte: str

@app.get("/messages")
async def lire_messages():
    return {"messages": base_de_donnees}

@app.post("/envoyer")
async def ajouter_message(msg: Message):
    base_de_donnees.append(msg.texte)
    return {"status": "Added successfully", "new_message": msg.text}