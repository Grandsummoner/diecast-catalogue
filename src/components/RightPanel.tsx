import React, { useState, useEffect, useMemo } from "react";
import { 
  Sparkles, BookOpen, Cpu, Trophy, Star, Edit, Loader2, AlertCircle
} from "lucide-react";
import { motion, AnimatePresence } from "motion/react";
import { DiecastCar, AINuggets } from "../types";
import { parseFilename } from "../data/cars";
import { generateClientFallbackNuggets } from "../utils/fallbackHistory";

interface RightPanelProps {
  cars: DiecastCar[];
  activeCar: DiecastCar | null;
  selectedIds: string[];
  onClearSelection: () => void;
  onSelectAll: (ids: string[]) => void;
  onUpdateAllCars: (cars: DiecastCar[]) => void;
  onUpdateTags: (id: string, tags: string[]) => void;
  onUpdateNotes: (id: string, notes: string) => void;
  onUpdateCarDetails?: (id: string, updates: Partial<DiecastCar>) => void;
  nuggetCache: Record<string, AINuggets>;
  setNuggetCache: React.Dispatch<React.SetStateAction<Record<string, AINuggets>>>;
  nuggetsLoading: boolean;
  nuggetsError: string | null;
  onRetryNuggets: () => void;
  onAddTagToCar?: (carId: string, tag: string) => void;
}

export default function RightPanel({
  cars,
  activeCar,
  selectedIds,
  onClearSelection,
  onSelectAll,
  onUpdateAllCars,
  onUpdateTags,
  onUpdateNotes,
  onUpdateCarDetails,
  nuggetCache,
  setNuggetCache,
  nuggetsLoading,
  nuggetsError,
  onRetryNuggets,
  onAddTagToCar
}: RightPanelProps) {
  const [activeTab, setActiveTab] = useState<"history" | "specs" | "nuggets" | null>("history");
  const [editNotes, setEditNotes] = useState("");

  const parsed = activeCar ? parseFilename(activeCar.filename) : null;
  const currentNuggets = activeCar ? nuggetCache[activeCar.filename] : null;

  const fallbackNuggets = useMemo(() => {
    if (!activeCar) return null;
    return generateClientFallbackNuggets(activeCar.filename);
  }, [activeCar]);

  const nuggetsToDisplay = currentNuggets || fallbackNuggets;

  const loading = nuggetsLoading;
  const error = nuggetsError;
  const handleRetryFetch = onRetryNuggets;

  // Reset states on active car change
  useEffect(() => {
    setActiveTab("history");
    if (activeCar) {
      setEditNotes(activeCar.notes || "");
    } else {
      setEditNotes("");
    }
  }, [activeCar?.id]);

  if (!activeCar || !parsed) {
    return (
      <div className="flex flex-col h-full bg-stone-900 border-l border-[#e4ded5] text-[#fdfbf7] p-6 items-center justify-center text-center">
        <Sparkles size={32} className="mb-3 text-stone-300 animate-pulse" />
        <h3 className="font-mono text-sm font-bold text-stone-700 tracking-wide">Model specifications</h3>
        <p className="text-xs max-w-[200px] mt-1.5 leading-relaxed text-stone-500">
          Select an active car file from the collection list to view history, specs, and collectors trivia nuggets.
        </p>
      </div>
    );
  }

  return (
    <div className="flex flex-col h-full bg-stone-900 border-l border-[#e4ded5] text-[#fdfbf7] overflow-hidden">
      
      {/* Encyclopedia Tab Accordions and Input Forms scroll section */}
      <div className="flex-1 overflow-y-auto custom-scrollbar p-4 space-y-4">
        
        {loading ? (
          /* ================= LOADING SCREEN SKELETON ================= */
          <div className="py-12 flex flex-col items-center justify-center text-center space-y-3.5">
            <Loader2 size={32} className="text-stone-700 animate-spin" />
            <div>
              <p className="text-xs font-mono text-stone-600 animate-pulse">Querying Gemini Expert Database...</p>
              <p className="text-[10px] text-stone-500 mt-1 font-mono">Parsing: "{parsed.fullName}"</p>
            </div>
            <div className="w-full max-w-[200px] bg-stone-100 border border-[#e4ded5] rounded-full h-1.5 overflow-hidden">
              <div className="bg-[#1a1a1a] h-full w-1/2 rounded-full animate-[loading_1.5s_infinite_ease-in-out]" />
            </div>
          </div>
        ) : nuggetsToDisplay ? (
          /* ================= ENCYCLOPEDIA TAB-BASED SYSTEM ================= */
          <div className="space-y-4">
            {error && (
              <div className="p-2.5 bg-amber-50/70 border border-amber-200 rounded-xl flex items-center justify-between gap-2 text-[10px] text-amber-800 font-mono animate-fade-in">
                <div className="flex items-center gap-1.5 truncate">
                  <AlertCircle size={12} className="text-amber-600 shrink-0" />
                  <span className="truncate">Local Archive (Live AI offline: Quota limits)</span>
                </div>
                <button
                  onClick={handleRetryFetch}
                  className="px-2 py-0.5 bg-white hover:bg-amber-100 border border-amber-300 rounded font-bold cursor-pointer transition-colors"
                >
                  Retry
                </button>
              </div>
            )}
            
            {/* Side-by-Side Latching Tabs */}
            <div className="grid grid-cols-3 gap-1 p-1 bg-stone-100 rounded-xl border border-[#e4ded5]">
              <button
                type="button"
                onClick={() => setActiveTab(activeTab === "history" ? null : "history")}
                className={`py-2 px-0.5 rounded-lg text-[10px] font-mono font-bold transition-all text-center flex flex-col items-center justify-center gap-1 cursor-pointer ${
                  activeTab === "history"
                    ? "bg-[#1a1a1a] text-[#faf6ee] shadow-xs"
                    : "text-stone-600 hover:bg-white/60"
                }`}
              >
                <BookOpen size={13} />
                <span className="truncate w-full px-0.5">Real History</span>
              </button>
              
              <button
                type="button"
                onClick={() => setActiveTab(activeTab === "specs" ? null : "specs")}
                className={`py-2 px-0.5 rounded-lg text-[10px] font-mono font-bold transition-all text-center flex flex-col items-center justify-center gap-1 cursor-pointer ${
                  activeTab === "specs"
                    ? "bg-[#1a1a1a] text-[#faf6ee] shadow-xs"
                    : "text-stone-600 hover:bg-white/60"
                }`}
              >
                <Cpu size={13} />
                <span className="truncate w-full px-0.5">Factory Specs</span>
              </button>

              <button
                type="button"
                onClick={() => setActiveTab(activeTab === "nuggets" ? null : "nuggets")}
                className={`py-2 px-0.5 rounded-lg text-[10px] font-mono font-bold transition-all text-center flex flex-col items-center justify-center gap-1 cursor-pointer ${
                  activeTab === "nuggets"
                    ? "bg-[#1a1a1a] text-[#faf6ee] shadow-xs"
                    : "text-stone-600 hover:bg-white/60"
                }`}
              >
                <Trophy size={13} />
                <span className="truncate w-full px-0.5">Nuggets</span>
              </button>
            </div>

            {/* Active Content Box (Only appears if a tab is latched, pushing content down) */}
            <AnimatePresence mode="wait">
              {activeTab && (
                <motion.div
                  key={activeTab}
                  initial={{ opacity: 0, height: 0 }}
                  animate={{ opacity: 1, height: "auto" }}
                  exit={{ opacity: 0, height: 0 }}
                  transition={{ duration: 0.15 }}
                  className="overflow-hidden border border-[#1a1a1a] rounded-xl bg-white shadow-xs"
                >
                  {activeTab === "history" && (
                    <div className="p-3 bg-white font-sans text-xs text-[#fdfbf7] leading-relaxed whitespace-pre-line text-justify">
                      {nuggetsToDisplay.history}
                    </div>
                  )}

                  {activeTab === "specs" && (
                    <div className="p-3 bg-white space-y-1.5">
                      {/* Model Name top row */}
                      <div className="flex justify-between items-center bg-stone-900 text-[#fdfbf7] px-2.5 py-2 rounded border border-[#1a1a1a] text-xs shadow-xs">
                        <span className="font-mono text-stone-300 text-[10px] uppercase font-bold">Model / Car</span>
                        <span className="font-bold text-right truncate max-w-[175px]" title={parsed.fullName}>{parsed.fullName}</span>
                      </div>

                      {nuggetsToDisplay.specs.map((spec, i) => (
                        <div key={i} className="flex justify-between items-center bg-stone-900 text-[#fdfbf7] px-2.5 py-2 rounded border border-[#1a1a1a] text-xs shadow-xs">
                          <span className="font-mono text-stone-300 text-[10px] uppercase font-bold truncate max-w-[100px]">{spec.label}</span>
                          <span className="font-bold text-right truncate max-w-[150px]" title={spec.value}>{spec.value}</span>
                        </div>
                      ))}
                    </div>
                  )}

                  {activeTab === "nuggets" && (
                    <div className="p-3 bg-white space-y-3.5">
                      
                      {/* Collectability Rating Box */}
                      <div className="bg-stone-900 border border-[#1a1a1a] rounded-xl p-3 space-y-2.5">
                        <div className="flex justify-between items-center">
                          <span className="text-[10px] font-mono text-stone-300 font-bold uppercase">Real car collectability:</span>
                          <span className="text-[10px] font-mono text-[#fdfbf7] font-bold bg-stone-900 px-2 py-0.5 rounded border border-[#1a1a1a]">
                            Score: {nuggetsToDisplay.collectorGuide.collectabilityScore}/10
                          </span>
                        </div>
                        <div className="w-full bg-stone-800 rounded-full h-1.5 overflow-hidden border border-[#1a1a1a]">
                          <div 
                            className="bg-stone-300 h-full rounded-full"
                            style={{ width: `${nuggetsToDisplay.collectorGuide.collectabilityScore * 10}%` }}
                          />
                        </div>
                      </div>

                      {/* Facts & Trivia */}
                      <div className="space-y-1.5">
                        <span className="block text-[10px] font-mono text-stone-300 font-bold uppercase tracking-wide">Notable trivia nuggets:</span>
                        <ul className="space-y-1.5">
                          {nuggetsToDisplay.facts.map((fact, i) => (
                            <li key={i} className="bg-stone-900 p-2.5 rounded-lg border border-[#1a1a1a] text-xs text-[#fdfbf7] leading-relaxed font-sans text-justify">
                              {fact}
                            </li>
                          ))}
                        </ul>
                      </div>

                    </div>
                  )}
                </motion.div>
              )}
            </AnimatePresence>

            {/* Curator Remarks Section */}
            <div className="pt-3 border-t border-[#e4ded5] space-y-2">
              <div className="flex items-center gap-1.5 text-[10px] font-mono text-stone-500 font-bold uppercase">
                <Edit size={11} className="text-[#1a1a1a]" />
                <span>Curator remarks</span>
              </div>
              <input
                type="text"
                value={editNotes}
                onChange={(e) => {
                  setEditNotes(e.target.value);
                  onUpdateNotes(activeCar.id, e.target.value);
                }}
                placeholder="Write comments, condition details, storage location..."
                className="w-full bg-stone-900 border border-[#1a1a1a] rounded-lg px-2.5 py-1.5 text-xs text-[#fdfbf7] placeholder-stone-400 focus:outline-none focus:border-stone-300 font-sans shadow-xs"
              />
            </div>

          </div>
        ) : (
          /* ================= FALLBACK FETCH INSTRUCTIONS ================= */
          <div className="py-6 text-center space-y-2.5">
            <Sparkles size={20} className="mx-auto text-stone-500 animate-pulse" />
            <p className="text-xs font-mono text-stone-500">Unlock automotive heritage, engine specs, and collector guides.</p>
            <button
              onClick={handleRetryFetch}
              className="px-3.5 py-1.5 bg-[#1a1a1a] hover:bg-[#2e2e2e] text-[#fdfbf7] font-bold rounded-lg text-xs font-mono cursor-pointer"
            >
              Analyze with Gemini AI
            </button>
          </div>
        )}

      </div>

    </div>
  );
}
