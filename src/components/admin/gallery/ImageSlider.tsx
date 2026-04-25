"use client";

import React, { useState, useRef, useEffect } from 'react';

interface ImageSliderProps {
    originalUrl: string;
    maskedUrl: string;
    aspectRatio?: string;
}

export default function ImageSlider({ originalUrl, maskedUrl, aspectRatio = "aspect-video" }: ImageSliderProps) {
    const [sliderPos, setSliderPos] = useState(50);
    const [isResizing, setIsResizing] = useState(false);
    const containerRef = useRef<HTMLDivElement>(null);

    const handleMove = (e: MouseEvent | TouchEvent) => {
        if (!isResizing || !containerRef.current) return;

        const rect = containerRef.current.getBoundingClientRect();
        const x = 'touches' in e ? e.touches[0].clientX : e.clientX;
        const relativeX = x - rect.left;
        const position = Math.max(0, Math.min(100, (relativeX / rect.width) * 100));
        
        setSliderPos(position);
    };

    const handleMouseDown = () => setIsResizing(true);
    const handleMouseUp = () => setIsResizing(false);

    useEffect(() => {
        window.addEventListener('mousemove', handleMove);
        window.addEventListener('mouseup', handleMouseUp);
        window.addEventListener('touchmove', handleMove);
        window.addEventListener('touchend', handleMouseUp);

        return () => {
            window.removeEventListener('mousemove', handleMove);
            window.removeEventListener('mouseup', handleMouseUp);
            window.removeEventListener('touchmove', handleMove);
            window.removeEventListener('touchend', handleMouseUp);
        };
    }, [isResizing]);

    return (
        <div 
            ref={containerRef}
            className={`relative w-full ${aspectRatio} overflow-hidden rounded-lg cursor-col-resize select-none border border-gray-200 shadow-inner bg-gray-100`}
            onMouseDown={handleMouseDown}
            onTouchStart={handleMouseDown}
        >
            {/* Background: Original */}
            <img 
                src={originalUrl} 
                alt="Original" 
                className="absolute inset-0 w-full h-full object-cover"
                draggable={false}
            />

            {/* Foreground: Masked */}
            <div 
                className="absolute inset-0 w-full h-full overflow-hidden"
                style={{ clipPath: `inset(0 ${100 - sliderPos}% 0 0)` }}
            >
                <img 
                    src={maskedUrl} 
                    alt="IA Mask" 
                    className="absolute inset-0 w-full h-full object-cover"
                    draggable={false}
                />
            </div>

            {/* Slider Handle */}
            <div 
                className="absolute inset-y-0 w-1 bg-white shadow-[0_0_10px_rgba(0,0,0,0.5)] z-10"
                style={{ left: `${sliderPos}%` }}
            >
                <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-8 h-8 bg-white rounded-full shadow-lg flex items-center justify-center">
                    <div className="flex gap-1">
                        <div className="w-0.5 h-3 bg-gray-400 rounded-full"></div>
                        <div className="w-0.5 h-3 bg-gray-400 rounded-full"></div>
                    </div>
                </div>
            </div>

            {/* Labels */}
            <div className="absolute bottom-4 left-4 px-2 py-1 bg-black/40 backdrop-blur-sm text-white text-[10px] uppercase tracking-widest rounded pointer-events-none">
                Original
            </div>
            <div className="absolute bottom-4 right-4 px-2 py-1 bg-blue-600/60 backdrop-blur-sm text-white text-[10px] uppercase tracking-widest rounded pointer-events-none">
                AI Vision
            </div>
        </div>
    );
}
