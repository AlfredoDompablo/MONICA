import Hero from "@/components/Hero";
import MapSection from "@/components/MapSection";
import StatisticsSection from "@/components/StatisticsSection";
import { NodeProvider } from "@/contexts/NodeContext";

/**
 * Página Principal (Home)
 * 
 * Renderiza el componente Hero como punto de entrada de la aplicación.
 */
export default function Home() {
  return (
    <main className="min-h-screen bg-white">
      <Hero />
      <NodeProvider>
        <MapSection />
        <StatisticsSection />
      </NodeProvider>
    </main>
  );
}
