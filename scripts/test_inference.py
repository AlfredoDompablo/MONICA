import requests
import base64
import sys
import os

def test_real_image(image_path, node_id="NODE_001"):
    if not os.path.exists(image_path):
        print(f"Error: No se encontró la imagen en {image_path}")
        return

    # 1. Leer imagen y convertir a Base64
    with open(image_path, "rb") as image_file:
        encoded_string = base64.b64encode(image_file.read()).decode('utf-8')

    # 2. Preparar el JSON para la API de Next.js
    payload = {
        "node_id": node_id,
        "image_original_base64": encoded_string
    }

    print(f"Enviando imagen {image_path} a la API de MONICA...")
    
    try:
        # 3. Enviar a Next.js (que lo pasará al servicio de IA en Python)
        response = requests.post(
            "http://localhost:3001/api/waste-detections",
            json=payload,
            timeout=30
        )

        if response.status_code == 201:
            result = response.json()
            print("\n✅ ¡Detección Procesada con Éxito!")
            print(f"ID de Detección: {result.get('detection_id')}")
            print(f"Cobertura: {result.get('coverage_percent')}%")
            print(f"Confianza: {result.get('confidence')}")
            print("\nAhora puedes verla en el Dashboard o en la página principal.")
        else:
            print(f"\n❌ Error {response.status_code}: {response.text}")

    except Exception as e:
        print(f"\n❌ Error de conexión: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: python test_inference.py <ruta_a_la_imagen> [node_id]")
    else:
        path = sys.argv[1]
        node = sys.argv[2] if len(sys.argv) > 2 else "NODE_001"
        test_real_image(path, node)
