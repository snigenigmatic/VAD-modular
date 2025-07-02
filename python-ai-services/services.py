import os
import torch
import logging
from dotenv import load_dotenv
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from transformers import AutoModelForCausalLM, AutoProcessor
import ollama

# --- Configuration and Logging ---
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger(__name__)
load_dotenv()

# --- Pydantic Models for API type safety ---
class ClipAnalysisRequest(BaseModel):
    clip_path: str
    prompt: str

class RefinementRequest(BaseModel):
    videollama_caption: str
    timestamp: float
    context: str

class FinalSummaryRequest(BaseModel):
    full_context: str


# --- AI Model Loader Class ---
class AIModels:
    def __init__(self):
        self.device = "cuda" if torch.cuda.is_available() else "cpu"
        logger.info(f"Device in use: {self.device}")
        self.model = None
        self.processor = None
        self.load_videollama()

    def load_videollama(self):
        model_id = "DAMO-NLP-SG/VideoLLaMA3-7B"
        cache_dir = os.getenv("HF_CACHE_DIR")
        logger.info(f"Loading Video-LLaMA 3 model: {model_id} from cache: {cache_dir}")
        try:
            self.model = AutoModelForCausalLM.from_pretrained(
                model_id,
                trust_remote_code=True,
                device_map="auto",
                torch_dtype=torch.bfloat16,
                attn_implementation="eager",
                cache_dir=cache_dir,
            )
            self.processor = AutoProcessor.from_pretrained(
                model_id, trust_remote_code=True, cache_dir=cache_dir
            )
            logger.info("Video-LLaMA 3 model loaded successfully.")
        except Exception as e:
            logger.critical(f"Failed to load Video-LLaMA 3 model: {e}", exc_info=True)
            raise

# --- FastAPI Application ---
app = FastAPI()

# Load models on startup
logger.info("Initializing AI Service...")
ai_models = AIModels()
logger.info("AI Service Initialized.")

@app.post("/analyze_clip")
def analyze_video_clip(request: ClipAnalysisRequest):
    """Endpoint to generate a caption for a video clip."""
    if not os.path.exists(request.clip_path):
        raise HTTPException(status_code=404, detail=f"Video clip not found at: {request.clip_path}")

    try:
        conversation = [
            {"role": "system", "content": "You are a helpful assistant analyzing video content."},
            {
                "role": "user",
                "content": [
                    {"type": "video", "video": {"video_path": request.clip_path, "fps": 1, "max_frames": 128}},
                    {"type": "text", "text": request.prompt},
                ],
            },
        ]
        inputs = ai_models.processor(conversation=conversation, return_tensors="pt")
        inputs = {k: v.to(ai_models.device) if isinstance(v, torch.Tensor) else v for k, v in inputs.items()}
        if "pixel_values" in inputs:
            inputs["pixel_values"] = inputs["pixel_values"].to(torch.bfloat16)

        with torch.no_grad():
            output_ids = ai_models.model.generate(**inputs, max_new_tokens=200)
        
        response = ai_models.processor.batch_decode(output_ids, skip_special_tokens=True)[0].strip()
        
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
            
        return {"caption": response}
    except Exception as e:
        logger.error(f"Video-LLaMA processing error for {request.clip_path}: {e}", exc_info=True)
        raise HTTPException(status_code=500, detail="Video-LLaMA processing failed.")

@app.post("/refine_analysis")
def refine_analysis(request: RefinementRequest):
    """Endpoint to refine a caption using a local LLM."""
    context_prompt = f"""
Previous video analysis context:
{request.context}

Current clip timestamp: {request.timestamp:.1f}s
Current clip description from Video-LLaMA: {request.videollama_caption}

You are a law enforcement officer analyzing surveillance footage.
Please analyze this video clip and provide:
1. A refined, clear description of what's happening
2. An anomaly score between 0.0 and 1.0
3. A brief explanation for the score
4. A simple 'yes' or 'no' for abnormal activity

Format your response as a JSON object with keys "description", "anomaly_score", "reasoning", "abnormal_activity".
Example: {{"description": "A red car is speeding through an intersection.", "anomaly_score": 0.8, "reasoning": "The car is moving significantly faster than other traffic.", "abnormal_activity": "yes"}}
"""
    try:
        response = ollama.chat(
            model="llama3", 
            messages=[{"role": "user", "content": context_prompt}],
            format="json" # Use Ollama's JSON mode for reliable output
        )
        refined_output = response["message"]["content"]
        return {"refined_analysis": refined_output}
    except Exception as e:
        logger.error(f"LLM refinement error: {e}", exc_info=True)
        raise HTTPException(status_code=500, detail="LLM refinement failed.")

@app.post("/summarize_report")
def summarize_report(request: FinalSummaryRequest):
    """Endpoint to generate the final summary report."""
    prompt = f"""
Analyze the following video analysis timeline and provide a comprehensive summary as a JSON object.

Video Analysis Timeline:
{request.full_context}

Provide a JSON object with keys "overall_summary", "key_events", "anomaly_summary", and "recommendations".
"""
    try:
        response = ollama.chat(
            model="llama3", 
            messages=[{"role": "user", "content": prompt}],
            format="json"
        )
        final_summary = response["message"]["content"]
        return {"final_summary": final_summary}
    except Exception as e:
        logger.error(f"Final summary generation error: {e}", exc_info=True)
        raise HTTPException(status_code=500, detail="Final summary generation failed.")

if __name__ == "__main__":
    import uvicorn
    # To run: uvicorn service:app --host 0.0.0.0 --port 8000
    uvicorn.run(app, host="0.0.0.0", port=8000)