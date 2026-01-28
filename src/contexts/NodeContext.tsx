"use client";

import { createContext, useContext, useState, ReactNode } from 'react';

interface NodeContextType {
    selectedNodeId: string | null;
    setSelectedNodeId: (id: string | null) => void;
}

const NodeContext = createContext<NodeContextType | undefined>(undefined);

export function NodeProvider({ children }: { children: ReactNode }) {
    const [selectedNodeId, setSelectedNodeId] = useState<string | null>(null);

    return (
        <NodeContext.Provider value={{ selectedNodeId, setSelectedNodeId }}>
            {children}
        </NodeContext.Provider>
    );
}

export function useNodeSelection() {
    const context = useContext(NodeContext);
    if (context === undefined) {
        throw new Error('useNodeSelection must be used within a NodeProvider');
    }
    return context;
}
