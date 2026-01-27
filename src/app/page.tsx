import Hero from "@/components/Hero";
import MapSection from "@/components/MapSection";

/**
 * Página Principal (Home)
 * 
 * Renderiza el componente Hero como punto de entrada de la aplicación.
 */
export default function Home() {
  return (
    <main>
      <Hero />
      <MapSection />
    </main>
  );
}
