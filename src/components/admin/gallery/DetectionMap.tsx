"use client";

import React from 'react';
import { MapContainer, TileLayer, Marker, Popup } from 'react-leaflet';
import L from 'leaflet';
import 'leaflet/dist/leaflet.css';

// Fix for default marker icon in Next.js
const iconUrl = 'https://unpkg.com/leaflet@1.7.1/dist/images/marker-icon.png';
const iconRetinaUrl = 'https://unpkg.com/leaflet@1.7.1/dist/images/marker-icon-2x.png';
const shadowUrl = 'https://unpkg.com/leaflet@1.7.1/dist/images/marker-shadow.png';

const sensorIcon = L.icon({
    iconUrl,
    iconRetinaUrl,
    shadowUrl,
    iconSize: [25, 41],
    iconAnchor: [12, 41],
    popupAnchor: [1, -34],
    shadowSize: [41, 41]
});

interface DetectionMapProps {
    latitude: number;
    longitude: number;
    description: string;
    nodeId: string;
}

export default function DetectionMap({ latitude, longitude, description, nodeId }: DetectionMapProps) {
    const position: [number, number] = [latitude, longitude];

    return (
        <div className="h-44 w-full rounded-xl overflow-hidden border border-gray-200 shadow-inner relative z-10">
            <MapContainer 
                center={position} 
                zoom={14} 
                scrollWheelZoom={false} 
                zoomControl={false}
                className="h-full w-full"
            >
                <TileLayer
                    attribution='&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>'
                    url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
                />
                <Marker position={position} icon={sensorIcon}>
                    <Popup>
                        <div className="text-xs">
                            <span className="font-bold text-[#1e3570]">{nodeId}</span>
                            <p className="text-[10px] text-gray-500 m-0">{description}</p>
                        </div>
                    </Popup>
                </Marker>
            </MapContainer>
        </div>
    );
}
