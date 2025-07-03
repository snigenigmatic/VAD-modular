# VAD-modular: AI-Powered Video Anomaly Detection System

A modular video analysis system that combines C++ orchestration with Python AI services to detect anomalies in surveillance footage using advanced multimodal models.

## 🏗️ Architecture Overview

VAD-modular consists of two main components working in tandem:

- **C++ Orchestrator** (`cpp_orchestrator/`): High-performance video processing engine that handles video segmentation, clip extraction, and API coordination
- **Python AI Services** (`python-ai-services/`): FastAPI-based microservice running Video-LLaMA 3 for video understanding and Ollama for analysis refinement

## 🚀 Features

- **Multimodal AI Analysis**: Leverages Video-LLaMA 3 (7B parameters) for advanced video understanding
- **Real-time Processing**: Efficient C++ orchestrator handles video segmentation and temporal processing  
- **Anomaly Detection**: Uses local LLMs to identify suspicious activities with scoring and reasoning
- **Modular Design**: Decoupled architecture allows independent scaling of components
- **Comprehensive Reporting**: Generates detailed JSON reports with timestamped analysis
- **GPU Acceleration**: Supports CUDA for accelerated AI inference

## 📁 Project Structure

```
VAD-modular/
├── cpp_orchestrator/           # C++ video processing engine
│   ├── src/main.cpp           # Main orchestration logic
│   ├── CMakeLists.txt         # Build configuration
│   └── build/                 # Build artifacts
│
├── python-ai-services/        # Python AI microservices
│   ├── services.py            # FastAPI service with AI models
│   ├── main.py                # Entry point
│   ├── pyproject.toml         # Python dependencies
│   └── requirements.txt       # Alternative pip requirements
│
├── reports/                   # Generated analysis reports
├── videos/                    # Input video files
└── README.md                  # This file
```

## 🔧 Prerequisites

### System Requirements
- **OS**: Linux (tested on Ubuntu/Debian-based systems)
- **GPU**: NVIDIA GPU with CUDA support (recommended)
- **RAM**: 16GB+ (Video-LLaMA 3 requires significant memory)
- **Storage**: 50GB+ free space for models and temporary files

### Dependencies

#### C++ Orchestrator
- CMake 3.15+
- C++17 compatible compiler (GCC 9+, Clang 10+)
- vcpkg package manager
- FFmpeg (for video processing)

#### Python AI Services  
- Python 3.8+
- CUDA Toolkit (for GPU acceleration)
- Ollama (for local LLM inference)

## 🛠️ Installation & Setup

### 1. Clone the Repository
```bash
git clone <repository-url>
cd VAD-modular
```

### 2. Setup Python AI Services

```bash
cd python-ai-services

# Install uv package manager (recommended)
curl -LsSf https://astral.sh/uv/install.sh | sh

# Create virtual environment and install dependencies
uv venv
source .venv/bin/activate
uv sync

# Alternative: using pip
# pip install -r requirements.txt
```

### 3. Install Ollama and Models
```bash
# Install Ollama
curl -fsSL https://ollama.ai/install.sh | sh

# Pull required model
ollama pull llama3
```

### 4. Setup Environment Variables
```bash
# Create .env file in python-ai-services/
echo "HF_CACHE_DIR=/path/to/huggingface/cache" > .env
# Optional: Set Hugging Face cache directory for model storage
```

### 5. Build C++ Orchestrator

```bash
cd cpp_orchestrator

# Install vcpkg (if not already installed)
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg

# Build the project
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 🚀 Usage

### 1. Start the AI Services
```bash
cd python-ai-services
source .venv/bin/activate
python services.py
# Service will start on http://127.0.0.1:8000
```

### 2. Run Video Analysis
```bash
cd cpp_orchestrator/build
./orchestrator /path/to/video.mp4 [clip_duration_seconds]

# Example:
./orchestrator ~/videos/surveillance.mp4 5.0
```

### 3. View Results
Analysis reports are saved to the `reports/` directory as JSON files with timestamps:
```
reports/
└── surveillance_20250702_143022.json
```

## 📊 API Endpoints

The Python AI service exposes three main endpoints:

### `/analyze_clip` (POST)
Analyzes a video clip using Video-LLaMA 3
```json
{
  "clip_path": "/path/to/clip.mp4",
  "prompt": "Describe what is happening in this video clip"
}
```

### `/refine_analysis` (POST)  
Refines analysis using local LLM with context
```json
{
  "videollama_caption": "Initial description",
  "timestamp": 15.5,
  "context": "Previous analysis context"
}
```

### `/summarize_report` (POST)
Generates final comprehensive report
```json
{
  "full_context": "Complete analysis timeline"
}
```

## 🔍 How It Works

1. **Video Segmentation**: C++ orchestrator splits input video into temporal clips (default 5s intervals)

2. **AI Analysis**: Each clip is processed by Video-LLaMA 3 to generate detailed descriptions

3. **Context Refinement**: Local LLM (Llama 3 via Ollama) analyzes descriptions with temporal context for anomaly detection

4. **Scoring & Classification**: System assigns anomaly scores (0.0-1.0) and binary classifications

5. **Report Generation**: Comprehensive JSON report with timeline, events, and recommendations

## 📈 Performance Notes

- **Video-LLaMA 3**: ~7B parameters, requires 14GB+ GPU memory
- **Processing Speed**: ~1-3 clips per minute (depending on hardware)
- **Recommended**: RTX 3090/4090 or better for optimal performance
- **CPU Fallback**: Available but significantly slower

## 🐛 Troubleshooting

### Common Issues

**CUDA Out of Memory**
```bash
# Reduce batch size or use CPU inference
export CUDA_VISIBLE_DEVICES=-1  # Force CPU mode
```

**FFmpeg Not Found**
```bash
# Install FFmpeg
sudo apt update && sudo apt install ffmpeg
```

**Model Download Fails**  
```bash
# Set Hugging Face cache directory
export HF_HOME=/path/with/enough/space
```

**Build Errors (C++)**
```bash
# Ensure vcpkg is properly configured
export VCPKG_ROOT=/path/to/vcpkg
```

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

## 🙏 Acknowledgments

- **Video-LLaMA 3**: DAMO-NLP-SG for the multimodal video understanding model
- **Ollama**: For providing easy local LLM inference
- **FastAPI**: For the robust API framework
- **vcpkg**: For C++ dependency management

## 📞 Support

For issues, questions, or contributions:
- Open an issue on GitHub
- Check the troubleshooting section above
- Review logs in both C++ and Python components

---

**Note**: This system is designed for research and development purposes. For production deployment, consider additional security, monitoring, and scaling measures.