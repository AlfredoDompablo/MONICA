import os
import uuid
import cv2
import numpy as np
from fastapi import FastAPI, UploadFile, File, Form
from ultralytics import YOLO
import boto3
from botocore.client import Config
from dotenv import load_dotenv
from io import BytesIO

load_dotenv()

app = FastAPI(title="MONICA AI Inference Service")

# Configuración MinIO / S3
s3_client = boto3.client(
    's3',
    endpoint_url=os.getenv('S3_ENDPOINT', 'http://localhost:9000'),
    aws_access_key_id=os.getenv('S3_ACCESS_KEY', 'admin'),
    aws_secret_access_key=os.getenv('S3_SECRET_KEY', 'monica_secret_123'),
    config=Config(signature_version='s3v4'),
    region_name=os.getenv('S3_REGION', 'us-east-1')
)
BUCKET_NAME = os.getenv('S3_BUCKET_NAME', 'detecciones')

# Cargar Modelo YOLOv11
# Se asume que el archivo best.pt está en la carpeta ai_service/model/
MODEL_PATH = "ai_service/model/best.pt"
if not os.path.exists(MODEL_PATH):
    # Intentar ruta relativa si falla
    MODEL_PATH = "model/best.pt"

try:
    model = YOLO(MODEL_PATH)
    print(f"✅ Modelo cargado exitosamente desde {MODEL_PATH}")
except Exception as e:
    print(f"❌ Error al cargar el modelo: {e}")
    model = None

@app.post("/process")
async def process_image(
    file: UploadFile = File(...),
    node_id: str = Form(...)
):
    if model is None:
        return {"error": "Modelo no cargado"}, 500

    # Leer imagen
    contents = await file.read()
    nparr = np.frombuffer(contents, np.uint8)
    img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
    img_orig = img.copy()

    # Ejecutar Inferencia
    results = model.predict(img, retina_masks=True, conf=0.25)
    result = results[0]

    # Generar Imagen con Máscara
    # Creamos una copia para la visualización (After)
    img_masked = img.copy()
    
    coverage_percent = 0
    confidence = 0
    
    if result.masks is not None:
        # Calcular porcentaje de cobertura
        # Combinamos todas las máscaras detectadas
        combined_mask = np.zeros(img.shape[:2], dtype=np.uint8)
        for mask in result.masks.data:
            m = mask.cpu().numpy().astype(np.uint8)
            # Redimensionar máscara al tamaño original si es necesario
            if m.shape != combined_mask.shape:
                m = cv2.resize(m, (combined_mask.shape[1], combined_mask.shape[0]))
            combined_mask = cv2.bitwise_or(combined_mask, m)
        
        # Calcular área
        total_pixels = combined_mask.size
        waste_pixels = np.count_nonzero(combined_mask)
        coverage_percent = round((waste_pixels / total_pixels) * 100, 2)
        
        # Dibujar máscaras en img_masked (efecto visual premium)
        # Usamos el método plot de ultralytics para algo rápido y bonito
        img_masked = result.plot(labels=True, boxes=False, masks=True)

    # Calcular confianza promedio
    if len(result.boxes) > 0:
        confidence = float(result.boxes.conf.mean())

    # Preparar subida a MinIO
    timestamp_str = cv2.getTickCount() # Generador simple de sufijo único
    date_folder = np.datetime64('now', 'D').astype(str)
    unique_id = uuid.uuid4()
    
    key_orig = f"{date_folder}/{node_id}_{unique_id}_orig.jpg"
    key_mask = f"{date_folder}/{node_id}_{unique_id}_mask.jpg"

    # Convertir imágenes a bytes para subir
    _, buffer_orig = cv2.imencode('.jpg', img_orig, [cv2.IMWRITE_JPEG_QUALITY, 85])
    _, buffer_mask = cv2.imencode('.jpg', img_masked, [cv2.IMWRITE_JPEG_QUALITY, 85])

    # Subir a MinIO
    s3_client.put_object(Bucket=BUCKET_NAME, Key=key_orig, Body=buffer_orig.tobytes(), ContentType='image/jpeg')
    s3_client.put_object(Bucket=BUCKET_NAME, Key=key_mask, Body=buffer_mask.tobytes(), ContentType='image/jpeg')

    return {
        "success": True,
        "node_id": node_id,
        "image_original": key_orig,
        "image_masked": key_mask,
        "coverage_percent": coverage_percent,
        "confidence": round(confidence, 2),
        "model_version": "YOLOv11-seg-MONICA"
    }

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
