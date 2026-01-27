"use client";

import { useEffect, useState } from 'react';
import { MapContainer, TileLayer, Marker, Popup } from 'react-leaflet';
import L from 'leaflet';
import 'leaflet/dist/leaflet.css';

// Fix for default marker icon in Next.js
const iconUrl = 'https://unpkg.com/leaflet@1.7.1/dist/images/marker-icon.png';
const iconRetinaUrl = 'https://unpkg.com/leaflet@1.7.1/dist/images/marker-icon-2x.png';
const shadowUrl = 'https://unpkg.com/leaflet@1.7.1/dist/images/marker-shadow.png';

const defaultIcon = L.icon({
    iconUrl,
    iconRetinaUrl,
    shadowUrl,
    iconSize: [25, 41],
    iconAnchor: [12, 41],
    popupAnchor: [1, -34],
    shadowSize: [41, 41]
});

// Custom Icon for Sensors
const sensorIcon = L.divIcon({
    className: 'custom-div-icon',
    html: `<div style="background-color: #1e3570; width: 1.5rem; height: 1.5rem; border-radius: 50%; border: 3px solid white; box-shadow: 0 4px 6px rgba(0,0,0,0.3);"></div>`,
    iconSize: [24, 24],
    iconAnchor: [12, 12]
});

interface Node {
    node_id: string;
    description: string;
    latitude: number;
    longitude: number;
    status: string;
    last_seen: string;
    sensor_readings?: {
        battery_level: number;
    }[];
}

interface MapProps {
    showDetails?: boolean;
}

const MapComponent = ({ showDetails = false }: MapProps) => {
    const [nodes, setNodes] = useState<Node[]>([]);
    const [loading, setLoading] = useState(true);

    useEffect(() => {
        const fetchNodes = async () => {
            try {
                const response = await fetch('/api/nodes');
                if (response.ok) {
                    const data = await response.json();
                    console.log("Map nodes data:", data); // Debugging
                    setNodes(data);
                }
            } catch (error) {
                console.error("Error fetching nodes:", error);
            } finally {
                setLoading(false);
            }
        };

        fetchNodes();
    }, []);

    // Center on Rio Magdalena (approximate central point or first node)
    const centerPosition: [number, number] = [19.3508, -99.1783]; // General Magdalena coordinates

    if (loading) {
        return <div className="h-full w-full flex items-center justify-center bg-gray-100 rounded-xl">Cargando mapa...</div>;
    }

    return (
        <MapContainer 
            center={centerPosition} 
            zoom={16} 
            scrollWheelZoom={true} 
            style={{ height: '100%', width: '100%', borderRadius: '0.75rem', zIndex: 0 }}
        >
            <TileLayer
                attribution='&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
                url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
            />
            
            {nodes.map((node) => (
                <Marker 
                    key={node.node_id} 
                    position={[node.latitude, node.longitude]}
                    icon={sensorIcon}
                >
                    <Popup>
                        <div className="p-2 min-w-[200px]">
                            <h3 className="font-bold text-[#1e3570] text-lg mb-1">{node.description}</h3>
                            <div className="text-sm text-gray-600 space-y-1">
                                <p><span className="font-semibold">ID:</span> {node.node_id}</p>
                                {showDetails && node.sensor_readings?.[0]?.battery_level !== undefined && (
                                    <p><span className="font-semibold">Batería:</span> {node.sensor_readings[0].battery_level}%</p>
                                )}
                                <p><span className="font-semibold">Última conexión:</span><br/> {node.last_seen ? new Date(node.last_seen).toLocaleString() : 'N/A'}</p>
                            </div>
                        </div>
                    </Popup>
                </Marker>
            ))}
        </MapContainer>
    );
};

export default MapComponent;
