import React, { useState, useRef } from "react";
import { 
  Star, Maximize2, Download, Layers, LayoutGrid, Sliders, Type, 
  Tag, Info, Minimize2, ChevronLeft, ChevronRight, X, Printer, HelpCircle,
  Bot, Loader2, Dices, Undo2, Redo2, RotateCcw, RotateCw, Plus, Minus,
  ChevronsUp, ChevronsDown, Eye, EyeOff, Play, Pause, ArrowLeft, ArrowRight,
  Shuffle, Repeat, PlayCircle
} from "lucide-react";
import { motion, AnimatePresence } from "motion/react";
import { DiecastCar, CollageTemplate, CollageBackground, CollageConfig, AINuggets } from "../types";
import { parseFilename } from "../data/cars";
import { generateClientFallbackNuggets } from "../utils/fallbackHistory";

interface MiddlePanelProps {
  selectedCars: DiecastCar[];
  activeCar: DiecastCar | null;
  onToggleFavorite: (id: string) => void;
  onRemoveFromSelection: (id: string) => void;
  onClearSelection: () => void;
  currentNuggets: AINuggets | null;
  nuggetsLoading: boolean;
  nuggetsError: string | null;
  onRetryNuggets: () => void;
  onAddTagToCar?: (carId: string, tag: string) => void;
  onUpdateTags: (id: string, tags: string[]) => void;
  totalCarsCount: number;
  filteredCars?: DiecastCar[];
  onSetActiveCarId?: (id: string) => void;
}

export default function MiddlePanel({
  selectedCars,
  activeCar,
  onToggleFavorite,
  onRemoveFromSelection,
  onClearSelection,
  currentNuggets,
  nuggetsLoading,
  nuggetsError,
  onRetryNuggets,
  onAddTagToCar,
  onUpdateTags,
  totalCarsCount,
  filteredCars = [],
  onSetActiveCarId
}: MiddlePanelProps) {
  const [isLightboxOpen, setIsLightboxOpen] = useState(false);
  const [isExporting, setIsExporting] = useState(false);
  const [focusedCarId, setFocusedCarId] = useState<string | null>(null);
  const pointerDownOnCardRef = React.useRef<boolean>(false);

  const [isInIframe, setIsInIframe] = useState(false);
  const [printError, setPrintError] = useState<string | null>(null);

  React.useEffect(() => {
    try {
      setIsInIframe(window.self !== window.top);
    } catch (e) {
      setIsInIframe(true);
    }
  }, []);

  // Per-photo custom tagging inline input states
  const [isAddingTag, setIsAddingTag] = useState(false);
  const [newTagInput, setNewTagInput] = useState("");

  const handleAddNewTag = (carId: string, currentTags: string[]) => {
    const trimmed = newTagInput.trim().toLowerCase();
    if (!trimmed) {
      setIsAddingTag(false);
      return;
    }
    const cleanTags = currentTags || [];
    if (cleanTags.length >= 3) return;
    if (!cleanTags.includes(trimmed)) {
      onUpdateTags(carId, [...cleanTags, trimmed]);
    }
    setNewTagInput("");
    setIsAddingTag(false);
  };

  const handleDeleteTag = (carId: string, currentTags: string[], tagToDelete: string) => {
    const cleanTags = currentTags || [];
    const updated = cleanTags.filter(t => t !== tagToDelete);
    onUpdateTags(carId, updated);
  };

  const collageCars = React.useMemo(() => {
    return selectedCars.slice(0, 10);
  }, [selectedCars]);

  const [collageConfig, setCollageConfig] = useState<CollageConfig>({
    template: "stacked",
    background: "minimal",
    showLabels: true,
    showScaleTags: true,
    borderColor: "#1a1a1a", // crisp black
    spacing: 16,
    title: "My Diecast Garage Collection",
    cardRadius: "lg",
    photoRatio: "auto",
    shadowSize: "md",
    textAlignment: "center",
    photoFit: "cover",
    isWhacky: false,
    diceVersion: 0,
    activeEffects: []
  });

  const [carLayouts, setCarLayouts] = useState<Record<string, {
    carId: string;
    x: number;
    y: number;
    width: number;
    rotation: number;
    zIndex: number;
    visible: boolean;
  }>>({});

  const activeCollageCars = React.useMemo(() => {
    return collageCars.filter(car => {
      const layout = carLayouts[car.id];
      return !layout || layout.visible !== false;
    });
  }, [collageCars, carLayouts]);

  const [slideshowActive, setSlideshowActive] = useState(false);
  const [slideshowDirection, setSlideshowDirection] = useState<'forward' | 'reverse' | 'random1' | 'random2'>('forward');
  const [slideshowSpeed, setSlideshowSpeed] = useState(2000);

  const slideshowList = React.useMemo(() => {
    if (selectedCars.length > 1) {
      return selectedCars;
    }
    return filteredCars.length > 0 ? filteredCars : (activeCar ? [activeCar] : []);
  }, [selectedCars, filteredCars, activeCar]);

  const currentSlideshowIdx = React.useMemo(() => {
    if (!activeCar) return 0;
    const idx = slideshowList.findIndex(car => car.id === activeCar.id);
    return idx >= 0 ? idx : 0;
  }, [slideshowList, activeCar]);

  const handleNextPhoto = React.useCallback(() => {
    if (slideshowList.length <= 1 || !onSetActiveCarId) return;
    const nextIdx = (currentSlideshowIdx + 1) % slideshowList.length;
    const nextCar = slideshowList[nextIdx];
    if (nextCar) {
      onSetActiveCarId(nextCar.id);
    }
  }, [slideshowList, currentSlideshowIdx, onSetActiveCarId]);

  const handlePrevPhoto = React.useCallback(() => {
    if (slideshowList.length <= 1 || !onSetActiveCarId) return;
    const prevIdx = (currentSlideshowIdx - 1 + slideshowList.length) % slideshowList.length;
    const prevCar = slideshowList[prevIdx];
    if (prevCar) {
      onSetActiveCarId(prevCar.id);
    }
  }, [slideshowList, currentSlideshowIdx, onSetActiveCarId]);

  // Keyboard events for fullscreen lightbox presentation slideshow
  React.useEffect(() => {
    if (!isLightboxOpen) return;

    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        setIsLightboxOpen(false);
      } else if (e.key === "ArrowRight") {
        handleNextPhoto();
      } else if (e.key === "ArrowLeft") {
        handlePrevPhoto();
      } else if (e.key === " ") {
        e.preventDefault();
        setSlideshowActive(prev => !prev);
      }
    };

    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [isLightboxOpen, handleNextPhoto, handlePrevPhoto]);

  // Slideshow play logic
  React.useEffect(() => {
    if (!slideshowActive || slideshowList.length <= 1) {
      return;
    }

    const intervalId = setInterval(() => {
      const len = slideshowList.length;
      let nextIdx = currentSlideshowIdx;

      if (slideshowDirection === 'forward') {
        nextIdx = (currentSlideshowIdx + 1) % len;
      } else if (slideshowDirection === 'reverse') {
        nextIdx = (currentSlideshowIdx - 1 + len) % len;
      } else if (slideshowDirection === 'random1') {
        nextIdx = Math.floor(Math.random() * len);
      } else if (slideshowDirection === 'random2') {
        if (len <= 1) {
          nextIdx = 0;
        } else {
          let attempts = 0;
          nextIdx = Math.floor(Math.random() * len);
          while (nextIdx === currentSlideshowIdx && attempts < 10) {
            nextIdx = Math.floor(Math.random() * len);
            attempts++;
          }
        }
      }

      const nextCar = slideshowList[nextIdx];
      if (nextCar && onSetActiveCarId) {
        onSetActiveCarId(nextCar.id);
      }
    }, slideshowSpeed);

    return () => clearInterval(intervalId);
  }, [
    slideshowActive,
    slideshowDirection,
    slideshowSpeed,
    slideshowList,
    currentSlideshowIdx,
    onSetActiveCarId
  ]);

  const getInitialLayoutForCar = (carId: string, idx: number) => {
    const cols = 3;
    const colIdx = idx % cols;
    const rowIdx = Math.floor(idx / cols);
    
    // Simple deterministic pseudo-random number based on string hash or index
    let hash = 0;
    for (let i = 0; i < carId.length; i++) {
      hash = carId.charCodeAt(i) + ((hash << 5) - hash);
    }
    const seed = (Math.abs(hash) % 1000) / 1000; // float between 0 and 1
    const seed2 = (Math.abs(hash * 31) % 1000) / 1000;
    const seed3 = (Math.abs(hash * 97) % 1000) / 1000;

    // Deterministic offsets within grid cells
    const randomXOffset = (seed * 16) - 8; // -8% to +8%
    const randomYOffset = (seed2 * 16) - 8; // -8% to +8%
    const defaultX = 22 + colIdx * 28 + randomXOffset;
    const defaultY = 32 + rowIdx * 18 + randomYOffset;
    
    // Deterministic rotation between -15 and 15 degrees
    const randomRotation = (seed3 * 30) - 15;
    
    // Deterministic width between 180px and 260px for variety
    const randomWidth = 180 + Math.floor(seed * 80);

    return {
      carId,
      x: Math.min(90, Math.max(10, defaultX)),
      y: Math.min(85, Math.max(30, defaultY)),
      width: randomWidth,
      rotation: randomRotation,
      zIndex: 10 + idx,
      visible: true
    };
  };

  React.useEffect(() => {
    setCarLayouts(prev => {
      const updated = { ...prev };
      let changed = false;
      selectedCars.forEach((car, idx) => {
        if (!updated[car.id]) {
          updated[car.id] = getInitialLayoutForCar(car.id, idx);
          changed = true;
        }
      });
      return changed ? updated : prev;
    });
  }, [selectedCars]);

  // Local undo/redo stacks for collageConfig
  const [configHistory, setConfigHistory] = useState<CollageConfig[]>([]);
  const [configRedoHistory, setConfigRedoHistory] = useState<CollageConfig[]>([]);

  const pushToHistory = (current: CollageConfig) => {
    setConfigHistory(prev => [...prev, JSON.parse(JSON.stringify(current))]);
    setConfigRedoHistory([]);
  };

  const handleConfigUndo = () => {
    if (configHistory.length === 0) return;
    const previous = configHistory[configHistory.length - 1];
    setConfigHistory(prev => prev.slice(0, -1));
    setConfigRedoHistory(prev => [...prev, JSON.parse(JSON.stringify(collageConfig))]);
    setCollageConfig(previous);
  };

  const handleConfigRedo = () => {
    if (configRedoHistory.length === 0) return;
    const next = configRedoHistory[configRedoHistory.length - 1];
    setConfigRedoHistory(prev => prev.slice(0, -1));
    setConfigHistory(prev => [...prev, JSON.parse(JSON.stringify(collageConfig))]);
    setCollageConfig(next);
  };

  const handleRandomizeCollage = () => {
    pushToHistory(collageConfig);

    const backgrounds: CollageBackground[] = ["slate", "dark", "leather", "carbon", "wood", "minimal", "sunset", "beach", "space", "vibrant"];
    const randomBackground = backgrounds[Math.floor(Math.random() * backgrounds.length)];

    const possibleEffects = ["rainbow", "comet", "star_trail", "glitter", "space_dust", "sunset", "vignette", "grain", "scanlines"];
    // Shuffle and pick 1 to 3 random effects
    const shuffled = [...possibleEffects].sort(() => 0.5 - Math.random());
    const count = Math.floor(Math.random() * 3) + 1; // 1 to 3 effects
    const randomEffects = shuffled.slice(0, count);

    const nextVersion = (collageConfig.diceVersion || 0) + 1;

    setCollageConfig(prev => ({
      ...prev,
      background: randomBackground,
      activeEffects: randomEffects,
      diceVersion: nextVersion
    }));

    // Randomize layout positions, rotations, and sizes of all collage cars!
    setCarLayouts(prev => {
      const updated = { ...prev };
      collageCars.forEach((car, idx) => {
        const cols = 3;
        const colIdx = idx % cols;
        const rowIdx = Math.floor(idx / cols);

        // Grid spacing with offsets
        const randomXOffset = (Math.random() * 20) - 10; // -10% to +10%
        const randomYOffset = (Math.random() * 20) - 10; // -10% to +10%
        const defaultX = 22 + colIdx * 28 + randomXOffset;
        const defaultY = 32 + rowIdx * 18 + randomYOffset;

        const randomRotation = (Math.random() * 40) - 20; // -20 to +20 degrees
        const randomWidth = 170 + Math.floor(Math.random() * 100); // 170px to 270px width

        updated[car.id] = {
          carId: car.id,
          x: Math.min(88, Math.max(12, defaultX)),
          y: Math.min(82, Math.max(30, defaultY)),
          width: randomWidth,
          rotation: randomRotation,
          zIndex: 10 + idx,
          visible: true
        };
      });
      return updated;
    });
  };

  const getStackedCardStyle = (idx: number, carId: string) => {
    const version = collageConfig.diceVersion || 0;
    const seed = Math.abs(Math.sin((idx + 1) * 32.4 + version * 77.1) * 1000);
    
    const anchors = [
      { x: 10, y: 15 }, // top left
      { x: 42, y: 10 }, // top center-left
      { x: 72, y: 14 }, // top center-right
      { x: 84, y: 15 }, // top right
      { x: 14, y: 48 }, // mid left
      { x: 45, y: 42 }, // center
      { x: 74, y: 52 }, // mid right
      { x: 18, y: 72 }, // bottom left
      { x: 50, y: 75 }, // bottom center
      { x: 78, y: 74 }, // bottom right
    ];
    
    const anchor = anchors[idx % anchors.length];
    const offsetX = ((seed * 1.5) % 10) - 5;
    const offsetY = ((seed * 2.5) % 10) - 5;
    
    const left = Math.max(2, Math.min(84, anchor.x + offsetX));
    const top = Math.max(2, Math.min(76, anchor.y + offsetY));
    const rot = ((seed * 4.3) % 30) - 15;
    const width = 170 + Math.floor((seed * 6.7) % 80);
    const isFocused = focusedCarId === carId;
    const zIndex = isFocused ? 50 : 5 + (idx % 10);
    
    return {
      position: "absolute" as const,
      left: `${left}%`,
      top: `${top}%`,
      width: `${width}px`,
      transform: `rotate(${rot}deg)`,
      zIndex: zIndex,
    };
  };

  const getWhackyCardStyle = (idx: number) => {
    if (!collageConfig.isWhacky) return {};
    const version = collageConfig.diceVersion || 0;
    
    // Slanting (rotation angle between -8 and 8 degrees)
    const rotDegrees = [-6, -4, -2, 2, 4, 6, -8, 8, -5, 5, -3, 3];
    const rotAngle = rotDegrees[(idx + version * 3) % rotDegrees.length];
    
    // Overlay translation offsets to partially overlap layout items
    const translateOffsets = [
      { x: -6, y: -4 },
      { x: 6, y: 4 },
      { x: -3, y: 6 },
      { x: 5, y: -6 },
      { x: 0, y: 0 },
      { x: -8, y: 3 },
      { x: 3, y: -8 }
    ];
    const offset = translateOffsets[(idx + version * 7) % translateOffsets.length];

    return {
      transform: `rotate(${rotAngle}deg) translate(${offset.x}px, ${offset.y}px)`,
      transition: "all 0.3s ease-in-out"
    };
  };

  const getWhackyCardClassName = (idx: number) => {
    if (!collageConfig.isWhacky) return "";
    const version = collageConfig.diceVersion || 0;
    
    // Size scaling classes (representing small and big sizes)
    const scales = ["scale-[0.93]", "scale-[0.97]", "scale-[1.00]", "scale-[1.03]", "scale-[1.06]"];
    const scaleClass = scales[(idx + version * 5) % scales.length];

    // Frame styles (representing photo framing)
    const frameStyles = [
      "border-4 border-white p-2.5 pb-7 bg-white shadow-md rounded-xs", // polaroid frame
      "border-2 border-amber-500 shadow-[0_0_12px_rgba(245,158,11,0.45)] bg-[#1a1a1a]", // neon amber border
      "border-[6px] border-[#442c1d] bg-stone-100 shadow-inner p-1.5", // thick warm wood border
      "border-2 border-yellow-500 ring-2 ring-yellow-300 ring-offset-1 p-1 bg-white shadow-lg", // gold trim
      "border-2 border-dashed border-stone-400 p-2 bg-stone-50/90", // vintage retro stamp
      "border-[3px] border-[#1a1a1a] p-1 shadow-2xl bg-white" // thick crisp outline
    ];
    const frameClass = frameStyles[(idx + version * 9) % frameStyles.length];

    return `${scaleClass} ${frameClass}`;
  };

  const getCardRadiusClass = (radius?: string) => {
    if (radius === "none") return "rounded-none";
    if (radius === "md") return "rounded-md";
    if (radius === "lg") return "rounded-lg";
    if (radius === "2xl") return "rounded-2xl";
    return "rounded-xl";
  };

  const getPhotoRatioClass = (ratio?: string, fallback = "aspect-video") => {
    if (ratio === "square") return "aspect-square";
    if (ratio === "video") return "aspect-video";
    return fallback;
  };

  const getShadowClass = (shadow?: string) => {
    if (shadow === "none") return "shadow-none";
    if (shadow === "sm") return "shadow-sm";
    if (shadow === "md") return "shadow-md";
    if (shadow === "xl") return "shadow-2xl";
    return "shadow-lg";
  };

  const getTextAlignmentClass = (alignment?: string) => {
    if (alignment === "left") return "text-left";
    if (alignment === "right") return "text-right";
    return "text-center";
  };

  const collageRef = useRef<HTMLDivElement>(null);
  const isMultiSelect = !slideshowActive && selectedCars.length > 1;
  const displayCar = activeCar || selectedCars[0] || null;

  const nuggetsToDisplay = React.useMemo(() => {
    if (currentNuggets) return currentNuggets;
    if (!displayCar) return null;
    return generateClientFallbackNuggets(displayCar.filename);
  }, [currentNuggets, displayCar]);

  // Ask Collector Bot states
  const [chatQuestion, setChatQuestion] = useState("");
  const [chatAnswer, setChatAnswer] = useState<string | null>(null);
  const [chatLoading, setChatLoading] = useState(false);

  // Clear chat when active car changes
  React.useEffect(() => {
    setChatQuestion("");
    setChatAnswer(null);
  }, [displayCar?.id]);

  // Ask Gemini Chat Handler
  const handleAskGemini = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!displayCar || !chatQuestion.trim()) return;

    setChatLoading(true);
    setChatAnswer(null);
    try {
      const parsed = parseFilename(displayCar.filename);
      const response = await fetch("/api/ask-gemini", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          filename: displayCar.filename,
          carName: parsed?.fullName,
          question: chatQuestion.trim()
        })
      });

      if (!response.ok) {
        throw new Error("Bot offline. Check connectivity.");
      }

      const data = await response.json();
      setChatAnswer(data.answer);
    } catch (err: any) {
      setChatAnswer(`Error querying bot: ${err.message}`);
    } finally {
      setChatLoading(false);
    }
  };

  // Background CSS Styles Mapper
  const getBgClass = (bg: CollageBackground) => {
    switch (bg) {
      case "slate":
        return "bg-gradient-to-br from-slate-900 via-slate-950 to-indigo-950 text-slate-100";
      case "dark":
        return "bg-slate-950 border border-slate-900 text-slate-100";
      case "leather":
        return "bg-gradient-to-br from-amber-950 via-[#2c1505] to-amber-950 text-slate-100";
      case "carbon":
        return "bg-slate-950 bg-[radial-gradient(#1e293b_1px,transparent_1px)] [background-size:16px_16px] text-slate-100";
      case "wood":
        return "bg-gradient-to-r from-amber-900 to-amber-950 text-slate-100";
      case "minimal":
        return "bg-[#faf6ee] text-[#1a1a1a] border border-[#e4ded5]";
      case "sunset":
        return "bg-gradient-to-br from-amber-500 via-rose-500 to-indigo-700 text-white shadow-xl";
      case "beach":
        return "bg-gradient-to-tr from-sky-300 via-cyan-400 to-amber-100 text-stone-900 border border-cyan-200 shadow-md";
      case "space":
        return "bg-gradient-to-tr from-slate-950 via-purple-950 to-indigo-950 text-slate-100 bg-[radial-gradient(ellipse_at_top,_var(--tw-gradient-stops))] from-indigo-900 via-purple-950 to-slate-950";
      case "vibrant":
        return "bg-gradient-to-br from-fuchsia-600 via-rose-500 to-amber-400 text-white shadow-xl";
      default:
        return "bg-[#faf6ee] text-[#1a1a1a]";
    }
  };

  // Beautiful in-app print-ready fullscreen poster overlay trigger
  const handleExportCollage = () => {
    setIsExporting(true);
    setPrintError(null);
  };

  const handlePrint = () => {
    try {
      setPrintError(null);
      window.print();
    } catch (err: any) {
      console.error("Print failed:", err);
      setPrintError("Print request failed. Please open the application in a new tab to bypass iframe sandboxing and print successfully.");
    }
  };

  const dragStartRef = React.useRef<{ id: string; startX: number; startY: number; initX: number; initY: number } | null>(null);

  const handlePointerDown = (e: React.PointerEvent, carId: string) => {
    // If clicking inside something with "no-drag" class, do not drag
    if ((e.target as HTMLElement).closest(".no-drag")) {
      return;
    }
    
    setFocusedCarId(carId);
    pointerDownOnCardRef.current = true;
    
    const layout = carLayouts[carId] || getInitialLayoutForCar(carId, collageCars.findIndex(c => c.id === carId));
    
    const container = collageRef.current;
    if (!container) return;
    
    dragStartRef.current = {
      id: carId,
      startX: e.clientX,
      startY: e.clientY,
      initX: layout.x,
      initY: layout.y
    };
    
    container.setPointerCapture(e.pointerId);
    e.stopPropagation();
  };

  const handlePointerMove = (e: React.PointerEvent) => {
    if (!dragStartRef.current) return;
    const { id, startX, startY, initX, initY } = dragStartRef.current;
    
    const container = collageRef.current;
    if (!container) return;
    
    const rect = container.getBoundingClientRect();
    
    const deltaX = ((e.clientX - startX) / rect.width) * 100;
    const deltaY = ((e.clientY - startY) / rect.height) * 100;
    
    const newX = Math.min(95, Math.max(5, initX + deltaX));
    const newY = Math.min(90, Math.max(26, initY + deltaY));
    
    setCarLayouts(prev => ({
      ...prev,
      [id]: {
        ...prev[id],
        x: newX,
        y: newY
      }
    }));
  };

  const handlePointerUp = (e: React.PointerEvent) => {
    if (!dragStartRef.current) return;
    const container = collageRef.current;
    if (container) {
      container.releasePointerCapture(e.pointerId);
    }
    dragStartRef.current = null;
  };

  const handleCanvasClick = (e: React.MouseEvent) => {
    if (pointerDownOnCardRef.current) {
      pointerDownOnCardRef.current = false;
      return;
    }
    if (e.target === e.currentTarget) {
      setFocusedCarId(null);
    }
  };

  const renderActiveEffects = (effects: string[] | undefined) => {
    if (!effects || effects.length === 0) return null;
    return (
      <div className="absolute inset-0 pointer-events-none overflow-hidden z-10 rounded-2xl">
        {effects.includes("rainbow") && (
          <div className="absolute inset-0 bg-gradient-to-tr from-red-500/10 via-yellow-400/10 via-green-500/10 via-blue-500/10 to-purple-500/10 animate-pulse mix-blend-screen" />
        )}
        {effects.includes("comet") && (
          <div className="absolute inset-0 bg-gradient-to-r from-transparent via-amber-400/20 to-transparent blur-[2px] animate-pulse" />
        )}
        {effects.includes("star_trail") && (
          <div className="absolute inset-0 bg-[radial-gradient(circle_at_30%_30%,rgba(251,191,36,0.15)_0%,transparent_50%),radial-gradient(circle_at_80%_70%,rgba(167,139,250,0.15)_0%,transparent_50%)]" />
        )}
        {effects.includes("glitter") && (
          <div className="absolute inset-0 opacity-20 bg-[radial-gradient(ellipse_at_center,_var(--tw-gradient-stops))] from-amber-300 via-transparent to-transparent animate-pulse" />
        )}
        {effects.includes("space_dust") && (
          <div className="absolute inset-0 bg-[radial-gradient(transparent_50%,rgba(15,23,42,0.3))] mix-blend-multiply" />
        )}
        {effects.includes("sunset") && (
          <div className="absolute inset-0 bg-gradient-to-b from-rose-500/10 via-orange-500/5 to-transparent mix-blend-overlay" />
        )}
        {effects.includes("vignette") && (
          <div className="absolute inset-0 bg-[radial-gradient(circle_at_center,transparent_40%,rgba(0,0,0,0.5))] mix-blend-multiply" />
        )}
        {effects.includes("grain") && (
          <div className="absolute inset-0 opacity-[0.03] bg-repeat pointer-events-none" style={{ backgroundImage: "url('data:image/svg+xml,%3Csvg viewBox=%220 0 200 200%22 xmlns=%22http://www.w3.org/2000/svg%22%3E%3Cfilter id=%22noise%22%3E%3CfeTurbulence type=%22fractalNoise%22 baseFrequency=%220.65%22 numOctaves=%223%22 stitchTiles=%22stitch%22/%3E%3C/filter%3E%3Crect width=%22100%25%22 height=%22100%25%22 filter=%22url(%23noise)%22/%3E%3C/svg%3E')" }} />
        )}
        {effects.includes("scanlines") && (
          <div className="absolute inset-0 pointer-events-none bg-[linear-gradient(rgba(18,16,16,0)_50%,rgba(0,0,0,0.25)_50%)] bg-[length:100%_4px]" />
        )}
      </div>
    );
  };

  const isLightBg = collageConfig.background === "minimal" || collageConfig.background === "beach";

  return (
    <div className="flex flex-col h-full bg-[#faf6ee] text-[#1a1a1a] overflow-hidden relative">
      
      {/* PERSISTENT SLIDESHOW PLAYER CONTROL ROW */}
      {displayCar && (
        <div className="p-3 bg-[#f6f2eb] border-b border-[#e4ded5] flex flex-wrap items-center justify-between gap-3 flex-shrink-0 z-10 shadow-xs">
          <div className="flex items-center gap-2">
            <span className="p-1.5 rounded-lg bg-[#1a1a1a]/10 text-[#1a1a1a]">
              <PlayCircle size={15} />
            </span>
            <div>
              <h3 className="text-xs font-bold font-mono text-stone-900 leading-tight">Slideshow Player</h3>
              <p className="text-[9px] text-stone-500 font-mono">
                {slideshowActive ? "Playing" : "Paused"} • {currentSlideshowIdx + 1} of {slideshowList.length} photo{slideshowList.length > 1 ? "s" : ""}
              </p>
            </div>
          </div>

          <div className="flex items-center gap-2 flex-wrap">
            {/* Play/Pause Button */}
            <button
              onClick={() => setSlideshowActive(!slideshowActive)}
              className={`px-3 py-1 text-xs font-mono font-bold flex items-center gap-1.5 transition-all cursor-pointer shadow-xs rounded-lg ${
                slideshowActive 
                  ? "bg-amber-600 text-[#faf6ee] hover:bg-amber-700 animate-pulse" 
                  : "bg-white text-stone-800 hover:bg-stone-50 border border-stone-300"
              }`}
            >
              {slideshowActive ? <Pause size={12} /> : <Play size={12} />}
              <span>{slideshowActive ? "Pause" : "Play"}</span>
            </button>

            {/* Directions Segment */}
            <div className="flex items-center gap-0.5 bg-stone-200/60 p-0.5 rounded-lg border border-stone-300/40">
              {/* Forward */}
              <button
                onClick={() => setSlideshowDirection('forward')}
                className={`p-1.5 rounded-md text-[10px] font-bold cursor-pointer transition-all flex items-center gap-1 ${
                  slideshowDirection === 'forward' 
                    ? "bg-[#1a1a1a] text-white shadow-xs" 
                    : "text-stone-600 hover:text-stone-900"
                }`}
                title="Forward Direction (Sequential)"
              >
                <ArrowRight size={12} />
                <span className="hidden md:inline">Forward</span>
              </button>

              {/* Reverse */}
              <button
                onClick={() => setSlideshowDirection('reverse')}
                className={`p-1.5 rounded-md text-[10px] font-bold cursor-pointer transition-all flex items-center gap-1 ${
                  slideshowDirection === 'reverse' 
                    ? "bg-[#1a1a1a] text-white shadow-xs" 
                    : "text-stone-600 hover:text-stone-900"
                }`}
                title="Reverse Direction (Sequential Backwards)"
              >
                <ArrowLeft size={12} />
                <span className="hidden md:inline">Reverse</span>
              </button>

              {/* Random 1 */}
              <button
                onClick={() => setSlideshowDirection('random1')}
                className={`p-1.5 rounded-md text-[10px] font-bold cursor-pointer transition-all flex items-center gap-1 ${
                  slideshowDirection === 'random1' 
                    ? "bg-[#1a1a1a] text-white shadow-xs" 
                    : "text-stone-600 hover:text-stone-900"
                }`}
                title="Random 1 (True Random Selection)"
              >
                <Shuffle size={12} />
                <span className="hidden md:inline">Random 1</span>
              </button>

              {/* Random 2 */}
              <button
                onClick={() => setSlideshowDirection('random2')}
                className={`p-1.5 rounded-md text-[10px] font-bold cursor-pointer transition-all flex items-center gap-1 ${
                  slideshowDirection === 'random2' 
                    ? "bg-[#1a1a1a] text-white shadow-xs" 
                    : "text-stone-600 hover:text-stone-900"
                }`}
                title="Random 2 Mode (Continuous No-repeat Random Selection)"
              >
                <Repeat size={12} />
                <span className="hidden md:inline">Random 2</span>
              </button>
            </div>

            {/* Speeds Segment */}
            <div className="flex items-center gap-1 bg-stone-200/60 p-0.5 rounded-lg border border-stone-300/40">
              {[1000, 2000, 3000, 5000].map(speed => (
                <button
                  key={speed}
                  onClick={() => setSlideshowSpeed(speed)}
                  className={`px-2 py-0.5 rounded text-[10px] font-mono font-bold cursor-pointer transition-all ${
                    slideshowSpeed === speed
                      ? "bg-[#1a1a1a] text-white shadow-xs"
                      : "text-stone-600 hover:text-stone-900"
                  }`}
                >
                  {speed / 1000}s
                </button>
              ))}
            </div>
          </div>
        </div>
      )}
      
      {/* Ask Collector Bot: Top of Middle Panel, Single Row with Robot Icon */}
      {displayCar && !isMultiSelect && (
        <div className="p-3 bg-[#fdfbf7] border-b border-[#e4ded5] flex flex-col gap-2 flex-shrink-0 z-10 shadow-xs">
          <form onSubmit={handleAskGemini} className="flex items-center gap-2.5 w-full">
            <Bot size={18} className="text-[#1a1a1a] flex-shrink-0" />
            <span className="font-mono text-[10px] font-bold uppercase tracking-wider text-stone-700 flex-shrink-0">Ask Collector Bot:</span>
            <input
              type="text"
              value={chatQuestion}
              onChange={(e) => setChatQuestion(e.target.value)}
              placeholder={`Ask about this ${parseFilename(displayCar.filename).make || "model"}...`}
              className="flex-1 bg-white border border-[#e4ded5] rounded-lg px-2.5 py-1 text-xs text-stone-800 placeholder-stone-400 focus:outline-none focus:border-[#1a1a1a] min-w-0"
              disabled={chatLoading}
            />
            <button
              type="submit"
              disabled={chatLoading || !chatQuestion.trim()}
              className="px-2.5 py-1 bg-[#1a1a1a] hover:bg-[#2e2e2e] text-[#fdfbf7] font-bold rounded-lg text-xs font-mono transition-all cursor-pointer flex-shrink-0 flex items-center gap-1"
            >
              {chatLoading ? (
                <Loader2 size={12} className="animate-spin text-stone-300" />
              ) : (
                <span>Ask</span>
              )}
            </button>
          </form>

          {/* Answer Box nested gracefully below it */}
          <AnimatePresence>
            {chatAnswer && (
              <motion.div
                initial={{ opacity: 0, height: 0 }}
                animate={{ opacity: 1, height: "auto" }}
                exit={{ opacity: 0, height: 0 }}
                className="border-t border-stone-200 pt-2 text-xs leading-relaxed text-stone-750 font-sans"
              >
                <div className="flex items-start gap-1.5">
                  <div className="bg-[#1a1a1a] text-[#faf6ee] px-1 py-0.5 rounded font-mono text-[8px] font-bold uppercase mt-0.5 flex-shrink-0">Answer</div>
                  <p className="whitespace-pre-line text-justify flex-1 text-[11px] pr-1">{chatAnswer}</p>
                </div>
              </motion.div>
            )}
          </AnimatePresence>
        </div>
      )}

      {/* Top Controller Bar (Only shown in Collage Composition mode to reclaim vertical space in single inspection view) */}
      {isMultiSelect && (
        <div className="p-4 border-b border-[#e4ded5] flex items-center justify-between flex-shrink-0 bg-[#faf6ee]/95 backdrop-blur-sm z-20">
          <div className="flex items-center gap-2">
            <div className="flex items-center gap-2">
              <span className="p-1.5 rounded-lg bg-[#1a1a1a]/15 text-[#1a1a1a]">
                <Layers size={18} />
              </span>
              <div>
                <h2 className="text-sm font-bold font-mono text-stone-900">Collage composition</h2>
                <p className="text-[10px] text-stone-500">
                  Assembling {collageCars.length} selected photographs
                  {selectedCars.length > 10 && <span className="text-amber-600 font-semibold ml-1">(First 10 of {selectedCars.length} shown)</span>}
                </p>
              </div>
            </div>
          </div>

          <div className="flex items-center gap-2">
            <button 
              onClick={onClearSelection}
              className="px-3 py-1.5 text-xs text-stone-600 hover:text-stone-900 bg-white hover:bg-stone-50 border border-[#e4ded5] rounded-lg transition-all cursor-pointer"
            >
              Cancel collage mode
            </button>
            
            {/* Undo Config */}
            <button
              onClick={handleConfigUndo}
              disabled={configHistory.length === 0}
              className="p-1.5 text-stone-600 hover:text-stone-950 bg-white hover:bg-stone-50 border border-[#e4ded5] rounded-lg disabled:opacity-40 disabled:cursor-not-allowed transition-all cursor-pointer"
              title={`Undo Layout (${configHistory.length} saved)`}
            >
              <Undo2 size={15} />
            </button>

            {/* Redo Config */}
            <button
              onClick={handleConfigRedo}
              disabled={configRedoHistory.length === 0}
              className="p-1.5 text-stone-600 hover:text-stone-950 bg-white hover:bg-stone-50 border border-[#e4ded5] rounded-lg disabled:opacity-40 disabled:cursor-not-allowed transition-all cursor-pointer"
              title="Redo Layout"
            >
              <Redo2 size={15} />
            </button>

            {/* Option to include/exclude filename (Squeezed before Dice Roll) */}
            <label className="flex items-center gap-2 cursor-pointer text-stone-700 bg-white border border-[#e4ded5] hover:bg-stone-50 px-2.5 py-1.5 rounded-lg text-xs font-mono font-medium select-none shadow-xs transition-all">
              <input
                type="checkbox"
                checked={collageConfig.showLabels}
                onChange={(e) => setCollageConfig(p => ({ ...p, showLabels: e.target.checked }))}
                className="accent-[#1a1a1a] h-3.5 w-3.5 rounded border-[#e4ded5]"
              />
              <span>Filename</span>
            </label>

            {/* Option to include/exclude scale tags */}
            <label className="flex items-center gap-2 cursor-pointer text-stone-700 bg-white border border-[#e4ded5] hover:bg-stone-50 px-2.5 py-1.5 rounded-lg text-xs font-mono font-medium select-none shadow-xs transition-all">
              <input
                type="checkbox"
                checked={collageConfig.showScaleTags}
                onChange={(e) => setCollageConfig(p => ({ ...p, showScaleTags: e.target.checked }))}
                className="accent-[#1a1a1a] h-3.5 w-3.5 rounded border-[#e4ded5]"
              />
              <span>Tags</span>
            </label>

            {/* Randomize Dice Icon */}
            <button
              onClick={handleRandomizeCollage}
              className="px-3 py-1.5 text-xs bg-amber-600 hover:bg-amber-700 text-white font-mono font-bold rounded-lg flex items-center gap-1.5 transition-all shadow-md cursor-pointer"
              title="Randomize whacky collage settings!"
            >
              <Dices size={15} />
              <span>Dice Roll</span>
            </button>

            <button
              onClick={handleExportCollage}
              className="px-3 py-1.5 text-xs bg-[#1a1a1a] hover:bg-[#2e2e2e] text-[#fdfbf7] font-bold rounded-lg flex items-center gap-1.5 transition-all shadow-md cursor-pointer"
            >
              <Download size={14} />
              <span>Export poster</span>
            </button>
          </div>
        </div>
      )}

      {/* Main Panel Content (Grid Scroll Container) */}
      <div className={`flex-1 p-6 pt-0 flex flex-col justify-between bg-[#faf6ee]/10 ${isMultiSelect ? "overflow-hidden" : "overflow-y-auto custom-scrollbar"}`}>
             {isMultiSelect ? (
          /* ================= MULTI SELECT COLLAGE BUILDER VIEW ================= */
          <div className="flex flex-col h-full overflow-hidden justify-center py-2">

            {/* COLLAGE CANVAS PREVIEW AREA */}
            <div className="flex-1 overflow-hidden p-2 md:p-4 bg-stone-900/5 rounded-2xl flex justify-center items-center">
              
              {/* WYSIWYG COLLAGE POSTER CONTAINER */}
              <div 
                className={`w-full max-w-4xl rounded-2xl p-6 sm:p-8 shadow-2xl relative border border-[#e4ded5]/60 overflow-hidden transition-all duration-300 ${getBgClass(collageConfig.background)} flex flex-col justify-between select-none max-h-full`}
                style={{
                  aspectRatio: "16 / 12.2",
                  height: "100%",
                }}
              >
                {/* Optional Active Effects */}
                {renderActiveEffects(collageConfig.activeEffects)}

                {/* Optional Editable Title */}
                <div className="text-center mb-6 border-b border-stone-200/20 pb-4 relative z-[2000]">
                  <input
                    type="text"
                    value={collageConfig.title}
                    onChange={(e) => setCollageConfig(p => ({ ...p, title: e.target.value }))}
                    className={`text-center bg-transparent border-b border-transparent hover:border-stone-400 focus:border-amber-500 focus:outline-none text-2xl font-bold font-mono tracking-wider w-full max-w-lg ${isLightBg ? "text-stone-900" : "text-white"}`}
                    placeholder="Give your collection poster a title..."
                  />
                </div>

                {/* RENDER DYNAMIC COLLAGE ITEMS BASED ON TEMPLATE */}
                {collageConfig.template === "stacked" ? (
                  <div 
                    ref={collageRef}
                    onClick={handleCanvasClick}
                    onPointerMove={handlePointerMove}
                    onPointerUp={handlePointerUp}
                    className="relative w-full aspect-[16/10] select-none overflow-hidden"
                  >
                    {collageCars.map((car, idx) => {
                      const layout = carLayouts[car.id] || getInitialLayoutForCar(car.id, idx);
                      if (layout.visible === false) return null;

                      const parsed = parseFilename(car.filename);
                      const isFocused = focusedCarId === car.id;

                      return (
                        <motion.div
                          key={car.id}
                          style={{
                            position: "absolute",
                            left: `${layout.x}%`,
                            top: `${layout.y}%`,
                            width: `${(layout.width / 600) * 100}%`,
                            transform: `translate(-50%, -50%) rotate(${layout.rotation}deg)`,
                            zIndex: isFocused ? 1000 : layout.zIndex,
                          }}
                          onPointerDown={(e) => handlePointerDown(e, car.id)}
                          className={`group cursor-pointer select-none overflow-hidden transition-all duration-150 border ${getCardRadiusClass(collageConfig.cardRadius)} ${getShadowClass(collageConfig.shadowSize)} ${
                            isFocused 
                              ? "ring-4 ring-amber-500 border-amber-500 bg-white" 
                              : `${isLightBg ? "bg-white border-[#e4ded5]" : "bg-slate-900 border-slate-800"}`
                          }`}
                        >
                          {/* Photo Header */}
                          <div className="relative bg-stone-50 aspect-video overflow-hidden">
                            <img 
                              src={car.imageUrl} 
                              className="h-full w-full object-cover pointer-events-none" 
                              alt="Model photo" 
                            />
                            {/* Small scale tag badge */}
                            {collageConfig.showScaleTags && (
                              <span className={`absolute bottom-1.5 right-1.5 px-1.5 py-0.5 font-mono text-[9px] font-semibold rounded border ${isLightBg ? "bg-stone-50/90 text-stone-900 border-[#e4ded5]" : "bg-slate-950/80 text-emerald-400 border-slate-800"}`}>
                                {car.scale}
                              </span>
                            )}
                          </div>

                          {/* Filename & Information Label Plate */}
                          {collageConfig.showLabels && (
                            <div className={`p-2 ${getTextAlignmentClass(collageConfig.textAlignment)} ${isLightBg ? "bg-white/95 border-t border-[#e4ded5]" : "bg-slate-900/90 border-t border-slate-800"}`}>
                              <p className={`font-mono text-[10px] font-bold truncate ${isLightBg ? "text-stone-900" : "text-white"}`} title={car.filename}>
                                {car.filename}
                              </p>
                              <p className="text-[9px] text-stone-500 truncate">{parsed.fullName}</p>
                            </div>
                          )}

                          {/* Direct manipulation floating overlay */}
                          {isFocused && (
                            <div 
                              className="absolute top-1 left-1 right-1 flex items-center justify-between gap-1 p-1 bg-stone-900/95 backdrop-blur-md text-white rounded-lg shadow-xl z-50 no-drag pointer-events-auto"
                              onPointerDown={(e) => e.stopPropagation()}
                              onClick={(e) => e.stopPropagation()}
                            >
                              <div className="flex items-center gap-0.5">
                                <button
                                  onClick={() => setCarLayouts(prev => ({
                                    ...prev,
                                    [car.id]: { ...prev[car.id], rotation: (prev[car.id].rotation - 15) }
                                  }))}
                                  className="p-1 bg-stone-800 hover:bg-stone-700 rounded text-stone-200 hover:text-amber-400 transition-all cursor-pointer"
                                  title="Rotate Left 15°"
                                >
                                  <RotateCcw size={10} />
                                </button>
                                <button
                                  onClick={() => setCarLayouts(prev => ({
                                    ...prev,
                                    [car.id]: { ...prev[car.id], rotation: (prev[car.id].rotation + 15) }
                                  }))}
                                  className="p-1 bg-stone-800 hover:bg-stone-700 rounded text-stone-200 hover:text-amber-400 transition-all cursor-pointer"
                                  title="Rotate Right 15°"
                                >
                                  <RotateCw size={10} />
                                </button>
                              </div>

                              <div className="flex items-center gap-0.5">
                                <button
                                  onClick={() => setCarLayouts(prev => ({
                                    ...prev,
                                    [car.id]: { ...prev[car.id], width: Math.max(120, prev[car.id].width - 30) }
                                  }))}
                                  className="p-1 bg-stone-800 hover:bg-stone-700 rounded text-stone-200 hover:text-amber-400 transition-all cursor-pointer"
                                  title="Shrink"
                                >
                                  <Minus size={10} />
                                </button>
                                <span className="text-[8px] font-mono font-bold text-stone-300 px-1">
                                  {layout.width}px
                                </span>
                                <button
                                  onClick={() => setCarLayouts(prev => ({
                                    ...prev,
                                    [car.id]: { ...prev[car.id], width: Math.min(450, prev[car.id].width + 30) }
                                  }))}
                                  className="p-1 bg-stone-800 hover:bg-stone-700 rounded text-stone-200 hover:text-amber-400 transition-all cursor-pointer"
                                  title="Enlarge"
                                >
                                  <Plus size={10} />
                                </button>
                              </div>

                              <div className="flex items-center gap-0.5">
                                <button
                                  onClick={() => {
                                    const layouts = Object.values(carLayouts) as Array<{ zIndex: number }>;
                                    const maxZ = Math.max(...layouts.map(l => l.zIndex), 10);
                                    setCarLayouts(prev => {
                                      const current = prev[car.id] || getInitialLayoutForCar(car.id, idx);
                                      return {
                                        ...prev,
                                        [car.id]: { ...current, zIndex: maxZ + 1 }
                                      };
                                    });
                                  }}
                                  className="p-1 bg-stone-800 hover:bg-stone-700 rounded text-stone-200 hover:text-amber-400 transition-all cursor-pointer"
                                  title="Bring to Front"
                                >
                                  <ChevronsUp size={10} />
                                </button>
                                <button
                                  onClick={() => {
                                    const layouts = Object.values(carLayouts) as Array<{ zIndex: number }>;
                                    const minZ = Math.min(...layouts.map(l => l.zIndex), 10);
                                    setCarLayouts(prev => {
                                      const current = prev[car.id] || getInitialLayoutForCar(car.id, idx);
                                      return {
                                        ...prev,
                                        [car.id]: { ...current, zIndex: Math.max(1, minZ - 1) }
                                      };
                                    });
                                  }}
                                  className="p-1 bg-stone-800 hover:bg-stone-700 rounded text-stone-200 hover:text-amber-400 transition-all cursor-pointer"
                                  title="Send to Back"
                                >
                                  <ChevronsDown size={10} />
                                </button>
                              </div>

                              <button
                                onClick={() => setCarLayouts(prev => ({
                                  ...prev,
                                  [car.id]: { ...prev[car.id], visible: false }
                                }))}
                                className="p-1 bg-red-950/60 hover:bg-red-600 rounded text-red-200 hover:text-white transition-all cursor-pointer ml-1"
                                title="Hide"
                              >
                                <X size={10} />
                              </button>
                            </div>
                          )}
                        </motion.div>
                      );
                    })}
                  </div>
                ) : collageConfig.template === "grid" ? (
                  <div 
                    className="grid grid-cols-2 md:grid-cols-3 gap-6 animate-fade-in flex-1 overflow-y-auto custom-scrollbar pr-1"
                    style={{ gap: `${collageConfig.spacing}px` }}
                  >
                    {collageCars.map((car, idx) => {
                      const layout = carLayouts[car.id] || getInitialLayoutForCar(car.id, idx);
                      if (layout.visible === false) return null;

                      const parsed = parseFilename(car.filename);
                      const isFocused = focusedCarId === car.id;
                      return (
                        <div
                          key={car.id}
                          onClick={() => setFocusedCarId(car.id)}
                          className={`overflow-hidden border cursor-pointer transition-all duration-150 relative ${getCardRadiusClass(collageConfig.cardRadius)} ${getShadowClass(collageConfig.shadowSize)} ${
                            isFocused 
                              ? "ring-4 ring-amber-500 border-amber-500 bg-white" 
                              : `${isLightBg ? "bg-white border-[#e4ded5]" : "bg-slate-900 border-slate-800"}`
                          }`}
                        >
                          <div className={`${getPhotoRatioClass(collageConfig.photoRatio, "aspect-video")} bg-stone-100 overflow-hidden relative`}>
                            <img src={car.imageUrl} className={`h-full w-full ${collageConfig.photoFit === "contain" ? "object-contain bg-stone-100" : "object-cover"}`} alt="Car" />
                            {collageConfig.showScaleTags && (
                              <span className={`absolute bottom-2 right-2 px-2 py-0.5 font-mono text-[10px] font-semibold rounded border ${isLightBg ? "bg-stone-50/90 text-stone-900 border-[#e4ded5]" : "bg-slate-950/80 text-emerald-400 border-slate-800"}`}>
                                {car.scale}
                              </span>
                            )}
                          </div>
                          {collageConfig.showLabels && (
                            <div className={`p-3 ${getTextAlignmentClass(collageConfig.textAlignment)}`}>
                              <p className={`font-mono text-xs truncate ${isLightBg ? "text-stone-900 font-bold" : "text-white"}`}>
                                {car.filename}
                              </p>
                              <p className="text-[10px] text-stone-500 font-sans mt-0.5">{parsed.fullName} • <span className={`font-bold font-mono text-[10px] ${isLightBg ? "text-stone-700" : "text-emerald-500"}`}>{car.brand}</span></p>
                            </div>
                          )}
                          {/* Hide button in grid if focused */}
                          {isFocused && (
                            <div className="absolute top-1 right-1 flex gap-1 z-50">
                              <button
                                onClick={(e) => {
                                  e.stopPropagation();
                                  setCarLayouts(prev => ({
                                    ...prev,
                                    [car.id]: { ...prev[car.id], visible: false }
                                  }));
                                  setFocusedCarId(null);
                                }}
                                className="p-1 bg-red-600 hover:bg-red-700 rounded text-white shadow-md cursor-pointer"
                                title="Hide"
                              >
                                <X size={10} />
                              </button>
                            </div>
                          )}
                        </div>
                      );
                    })}
                  </div>
                ) : (
                  <div className="space-y-6 animate-fade-in flex-1 overflow-y-auto custom-scrollbar pr-1">
                    <div className={`flex justify-around select-none ${isLightBg ? "text-stone-300" : "text-slate-800"}`}>
                      {Array.from({ length: 16 }).map((_, i) => (
                        <div key={i} className="h-3 w-5 bg-current rounded-sm" />
                      ))}
                    </div>
                    <div className="grid grid-cols-2 gap-4">
                      {collageCars.map((car, idx) => {
                        const layout = carLayouts[car.id] || getInitialLayoutForCar(car.id, idx);
                        if (layout.visible === false) return null;

                        const parsed = parseFilename(car.filename);
                        const isFocused = focusedCarId === car.id;
                        return (
                          <div 
                            key={car.id}
                            onClick={() => setFocusedCarId(car.id)}
                            className={`overflow-hidden border cursor-pointer transition-all duration-150 relative ${getCardRadiusClass(collageConfig.cardRadius)} ${getShadowClass(collageConfig.shadowSize)} ${
                              isFocused 
                                ? "ring-4 ring-amber-500 border-amber-500 bg-white" 
                                : `${isLightBg ? "bg-white border-[#e4ded5]" : "bg-slate-900 border-slate-800"}`
                            }`}
                          >
                            <div className={`${getPhotoRatioClass(collageConfig.photoRatio, "aspect-video")} relative bg-stone-100`}>
                              <img src={car.imageUrl} className={`h-full w-full ${collageConfig.photoFit === "contain" ? "object-contain bg-stone-100" : "object-cover"}`} alt="Car" />
                            </div>
                            {collageConfig.showLabels && (
                              <div className={`p-3 ${getTextAlignmentClass(collageConfig.textAlignment)}`}>
                                <p className={`font-mono text-xs truncate ${isLightBg ? "text-stone-900 font-bold" : "text-emerald-400"}`}>{car.filename}</p>
                                <p className="text-[10px] text-stone-500 mt-1">
                                  {parsed.fullName}{collageConfig.showScaleTags ? ` (${car.scale})` : ""}
                                </p>
                              </div>
                            )}
                            {/* Hide button in strip if focused */}
                            {isFocused && (
                              <div className="absolute top-1 right-1 flex gap-1 z-50">
                                <button
                                  onClick={(e) => {
                                    e.stopPropagation();
                                    setCarLayouts(prev => ({
                                      ...prev,
                                      [car.id]: { ...prev[car.id], visible: false }
                                    }));
                                    setFocusedCarId(null);
                                  }}
                                  className="p-1 bg-red-600 hover:bg-red-700 rounded text-white shadow-md cursor-pointer"
                                  title="Hide"
                                >
                                  <X size={10} />
                                </button>
                              </div>
                            )}
                          </div>
                        );
                      })}
                    </div>
                    <div className={`flex justify-around select-none ${isLightBg ? "text-stone-300" : "text-slate-800"}`}>
                      {Array.from({ length: 16 }).map((_, i) => (
                        <div key={i} className="h-3 w-5 bg-current rounded-sm" />
                      ))}
                    </div>
                  </div>
                )}

                {/* Poster Footer Badging */}
                <div className="mt-4 pt-3 sm:mt-12 sm:pt-6 border-t border-stone-200/20 flex flex-wrap items-center justify-between gap-4 text-[10px] font-mono tracking-widest uppercase">
                  <span className={isLightBg ? "text-stone-500" : "text-stone-400"}>
                    Registered Collector Poster Presentation
                  </span>
                  <span className={`font-bold ${isLightBg ? "text-stone-900" : "text-amber-400"}`}>
                    Diecast Blueprint Studio
                  </span>
                </div>
              </div>
            </div>



          </div>
        ) : displayCar ? (
          /* ================= SINGLE PHOTOS INSPECTION CANVAS (CREATIVE PRESENTATION POSTER) ================= */
          <div className="flex-1 flex flex-col justify-start items-center pt-0 pb-4 px-2">
            
            <motion.div
              key={displayCar.id}
              initial={{ opacity: 0, y: 15 }}
              animate={{ opacity: 1, y: 0 }}
              className="w-full max-w-2xl bg-[#faf6ee] border-2 border-[#1a1a1a] rounded-xl overflow-hidden shadow-xl text-[#1a1a1a]"
            >
              {/* Technical Header Block */}
              <div className="border-b-2 border-[#1a1a1a] p-4 bg-[#faf6ee] flex flex-col gap-2 font-mono text-xs">
                <div className="flex flex-col">
                  <span className="text-[10px] text-stone-500 uppercase tracking-widest">Diecast Identifier</span>
                  <span className="font-bold uppercase tracking-tight text-stone-900 truncate max-w-full">{displayCar.filename}</span>
                </div>
                {/* Dedicated Tags Row */}
                <div className="flex flex-wrap items-center gap-1.5 pt-1.5 border-t border-stone-200/40 min-h-[32px]">
                  <span className="text-[9px] font-bold text-stone-400 uppercase tracking-wider mr-1">Tags:</span>
                  
                  {/* Custom Tags */}
                  {displayCar.tags && displayCar.tags.map(tag => (
                    <span key={tag} className="px-2 py-0.5 border border-[#1a1a1a] text-[#1a1a1a] rounded-md font-bold text-[9px] uppercase font-mono bg-white flex items-center gap-1 group/tag">
                      <span>🏷️</span>
                      <span className="truncate max-w-[80px]" title={tag}>{tag}</span>
                      <button
                        type="button"
                        onClick={() => handleDeleteTag(displayCar.id, displayCar.tags, tag)}
                        className="text-[#1a1a1a] hover:text-red-600 font-bold ml-1 cursor-pointer transition-colors text-[10px]"
                        title={`Remove tag ${tag}`}
                      >
                        ×
                      </button>
                    </span>
                  ))}

                  {/* Input field or + Add Tag button */}
                  {(!displayCar.tags || displayCar.tags.length < 3) && (
                    isAddingTag ? (
                      <div className="flex items-center gap-1 animate-fade-in">
                        <input
                          type="text"
                          value={newTagInput}
                          onChange={(e) => setNewTagInput(e.target.value.slice(0, 12))}
                          maxLength={12}
                          placeholder="tag..."
                          autoFocus
                          onKeyDown={(e) => {
                            if (e.key === "Enter") {
                              handleAddNewTag(displayCar.id, displayCar.tags || []);
                            } else if (e.key === "Escape") {
                              setIsAddingTag(false);
                            }
                          }}
                          onBlur={() => {
                            setTimeout(() => {
                              handleAddNewTag(displayCar.id, displayCar.tags || []);
                            }, 180);
                          }}
                          className="px-1.5 py-0.5 bg-white border border-[#1a1a1a] rounded text-[9px] font-mono focus:outline-none w-16 focus:ring-1 focus:ring-[#1a1a1a]"
                        />
                        <button
                          type="button"
                          onClick={() => handleAddNewTag(displayCar.id, displayCar.tags || [])}
                          className="text-[9px] font-bold text-emerald-600 hover:text-emerald-700 font-mono px-0.5"
                        >
                          ✓
                        </button>
                        <button
                          type="button"
                          onClick={() => setIsAddingTag(false)}
                          className="text-[9px] font-bold text-red-600 hover:text-red-700 font-mono px-0.5"
                        >
                          ×
                        </button>
                      </div>
                    ) : (
                      <button
                        type="button"
                        onClick={() => {
                          setNewTagInput("");
                          setIsAddingTag(true);
                        }}
                        className="px-2 py-0.5 border border-dashed border-stone-300 hover:border-[#1a1a1a] hover:text-[#1a1a1a] text-stone-500 rounded-md font-bold text-[9px] font-mono bg-white/40 flex items-center gap-0.5 transition-colors cursor-pointer"
                        title="Add Tag (max 3 tags per photo, max 12 chars each)"
                      >
                        <span className="text-xs">+</span>
                        <span>Add</span>
                      </button>
                    )
                  )}

                  {(!displayCar.tags || displayCar.tags.length === 0) && !displayCar.category && !displayCar.brand && !displayCar.scale && !isAddingTag && (
                    <span className="text-[9px] text-stone-400 font-mono italic">no tags</span>
                  )}
                </div>
              </div>

              {/* Museum-Grade Matting Framed Image */}
              <div className="p-6 bg-stone-50/50 border-b-2 border-[#1a1a1a] relative flex justify-center items-center">
                
                {/* Floating interactions */}
                <div className="absolute top-4 right-4 flex items-center gap-2 z-10">
                  <button
                    onClick={() => onToggleFavorite(displayCar.id)}
                    className={`p-2 rounded-full transition-all border-2 border-[#1a1a1a] cursor-pointer shadow-sm ${displayCar.isFavorite ? "bg-amber-400 text-[#1a1a1a]" : "bg-white text-stone-600 hover:text-amber-600"}`}
                    title="Star this photo"
                  >
                    <Star size={15} fill={displayCar.isFavorite ? "currentColor" : "none"} className="stroke-[2.5px]" />
                  </button>

                  <button
                    onClick={() => setIsLightboxOpen(true)}
                    className="p-2 rounded-full bg-white border-2 border-[#1a1a1a] text-stone-600 hover:text-[#1a1a1a] transition-all cursor-pointer shadow-sm"
                    title="Maximize Photo"
                  >
                    <Maximize2 size={15} className="stroke-[2.5px]" />
                  </button>
                </div>

                <div 
                  onDragOver={(e) => {
                    e.preventDefault();
                    e.currentTarget.classList.add("ring-2", "ring-amber-500", "border-amber-500");
                  }}
                  onDragLeave={(e) => {
                    e.currentTarget.classList.remove("ring-2", "ring-amber-500", "border-amber-500");
                  }}
                  onDrop={(e) => {
                    e.preventDefault();
                    e.currentTarget.classList.remove("ring-2", "ring-amber-500", "border-amber-500");
                    const tag = e.dataTransfer.getData("text/plain");
                    if (tag && onAddTagToCar) {
                      onAddTagToCar(displayCar.id, tag);
                    }
                  }}
                  className="w-full aspect-[16/10] bg-white border-2 border-[#1a1a1a] rounded-lg shadow-md p-4 flex items-center justify-center relative overflow-hidden group"
                >
                  <img 
                    src={displayCar.imageUrl} 
                    alt="Inspected diecast model"
                    referrerPolicy="no-referrer"
                    className="h-full w-full object-contain group-hover:scale-[1.02] transition-transform duration-300"
                  />
                </div>
              </div>

              {/* Typographic Poster Info Stack */}
              <div className="flex flex-col border-t border-[#1a1a1a] bg-[#faf6ee] divide-y divide-[#1a1a1a]">
                
                {/* Curator Notes & Archive Metadata (Full Width) */}
                <div className="p-5 space-y-3">
                  {/* Personal Curator's Notes */}
                  {displayCar.notes ? (
                    <div>
                      <span className="text-[10px] font-mono font-bold text-stone-500 uppercase tracking-widest block mb-1">
                        Curator's Notes:
                      </span>
                      <p className="text-xs text-stone-700 font-serif leading-relaxed italic bg-white/40 p-2.5 rounded-lg border border-stone-200">
                        "{displayCar.notes}"
                      </p>
                    </div>
                  ) : (
                    <p className="text-xs text-stone-500 italic font-mono">No curator notes registered for this model.</p>
                  )}

                  {/* Registered Timestamp */}
                  <div className="pt-2 text-[10px] font-mono text-stone-400 border-t border-dashed border-stone-200">
                    CATALOGUE ENTRY: {displayCar.dateAdded}
                  </div>
                </div>

              </div>

            </motion.div>
          </div>
        ) : (
          /* ================= EMPTY STATE / SELECTION PROMPT / UPLOAD PROMPT ================= */
          totalCarsCount > 0 ? (
            <div className="flex-1 flex flex-col items-center justify-center text-center p-8 text-stone-500 min-h-[400px]">
              <span className="text-4xl mb-4 animate-bounce">🚗</span>
              <h3 className="text-sm font-bold text-[#1a1a1a] font-mono uppercase tracking-wider">No File Selected</h3>
              <p className="text-xs max-w-xs mt-1.5 leading-relaxed text-stone-600">
                Pick a scale model photograph from the collection checklist on the left to start inspecting and customizing!
              </p>
            </div>
          ) : (
            <div className="flex-1 flex flex-col items-center justify-center text-center p-8 text-stone-500 min-h-[400px]">
              <Layers size={42} className="mb-4 text-[#1a1a1a]/40" />
              <h3 className="text-sm font-bold text-[#1a1a1a] font-mono uppercase tracking-wider">No Photographs Loaded</h3>
              <p className="text-xs max-w-xs mt-1.5 leading-relaxed text-stone-600">
                Select or drop your scale model photographs folder in the sidebar. The cataloguer will instantly parse and extract models from your files!
              </p>
            </div>
          )
        )}

      </div>

      {/* FULLSCREEN LIGHTBOX PORTAL */}
      <AnimatePresence>
        {isLightboxOpen && displayCar && (
          <motion.div 
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            className="fixed inset-0 bg-[#fdfbf7]/98 backdrop-blur-xs z-[10000] flex flex-col justify-between p-6 select-none"
            onClick={() => setIsLightboxOpen(false)}
          >
            {/* Top Bar Header */}
            <div 
              className="w-full flex items-center justify-between z-10"
              onClick={(e) => e.stopPropagation()}
            >
              <div className="flex flex-col">
                <span className="font-mono text-stone-500 text-[10px] font-bold uppercase tracking-widest">
                  Fullscreen Slideshow Player
                </span>
                <h4 className="text-xs font-bold font-mono text-stone-900 mt-0.5">
                  {currentSlideshowIdx + 1} of {slideshowList.length} • {slideshowActive ? "Playing" : "Paused"}
                </h4>
              </div>

              <div className="flex items-center gap-2">
                <button 
                  onClick={() => setIsLightboxOpen(false)}
                  className="p-2.5 bg-white text-stone-800 border-2 border-[#1a1a1a] hover:bg-stone-50 rounded-full cursor-pointer shadow-sm flex items-center justify-center transition-all"
                  title="Close Fullscreen (Esc)"
                >
                  <Minimize2 size={18} className="stroke-[2.5px]" />
                </button>
              </div>
            </div>

            {/* Middle Row with Image and Left/Right Hover Nav buttons */}
            <div className="flex-1 flex items-center justify-center relative w-full my-4">
              {/* Prev Button */}
              {slideshowList.length > 1 && (
                <button
                  type="button"
                  onClick={(e) => {
                    e.stopPropagation();
                    handlePrevPhoto();
                  }}
                  className="absolute left-2 md:left-6 p-4 rounded-full bg-white/90 border-2 border-[#1a1a1a] text-stone-800 hover:bg-stone-50 hover:scale-105 active:scale-95 transition-all cursor-pointer shadow-lg z-20"
                  title="Previous Photo (Left Arrow)"
                >
                  <ChevronLeft size={24} className="stroke-[2.5px]" />
                </button>
              )}

              {/* Main Image Container */}
              <div 
                className="max-w-5xl max-h-[65vh] md:max-h-[70vh] relative flex items-center justify-center"
                onClick={(e) => e.stopPropagation()}
              >
                <img 
                  src={displayCar.imageUrl} 
                  className="max-w-full max-h-[65vh] md:max-h-[70vh] object-contain rounded-xl shadow-2xl border-2 border-[#1a1a1a] bg-white p-4" 
                  alt="Fullscreen viewer"
                />
              </div>

              {/* Next Button */}
              {slideshowList.length > 1 && (
                <button
                  type="button"
                  onClick={(e) => {
                    e.stopPropagation();
                    handleNextPhoto();
                  }}
                  className="absolute right-2 md:right-6 p-4 rounded-full bg-white/90 border-2 border-[#1a1a1a] text-stone-800 hover:bg-stone-50 hover:scale-105 active:scale-95 transition-all cursor-pointer shadow-lg z-20"
                  title="Next Photo (Right Arrow)"
                >
                  <ChevronRight size={24} className="stroke-[2.5px]" />
                </button>
              )}
            </div>

            {/* Bottom Panel Info & Controls */}
            <div 
              className="w-full flex flex-col items-center gap-4 z-10"
              onClick={(e) => e.stopPropagation()}
            >
              {/* Meta information card */}
              <div className="text-center max-w-2xl bg-white p-4 rounded-xl border-2 border-[#1a1a1a] shadow-md w-full">
                <span className="font-mono text-stone-600 text-[10px] font-bold block uppercase tracking-wider mb-0.5">
                  {parseFilename(displayCar.filename).fullName}
                </span>
                <span className="font-mono text-stone-400 text-[9px] block">
                  {displayCar.filename}
                </span>
                {displayCar.notes && (
                  <p className="mt-2 text-stone-600 text-xs font-light max-h-[60px] overflow-y-auto pr-1">
                    {displayCar.notes}
                  </p>
                )}
              </div>

              {/* Slideshow controller HUD */}
              <div className="bg-[#f6f2eb] border-2 border-[#1a1a1a] px-4 py-2.5 rounded-2xl flex flex-wrap items-center justify-center gap-3.5 shadow-md">
                {/* Play/Pause */}
                <button
                  onClick={() => setSlideshowActive(!slideshowActive)}
                  className={`px-4 py-1.5 text-xs font-mono font-bold flex items-center gap-2 transition-all cursor-pointer shadow-sm rounded-lg border border-[#1a1a1a] ${
                    slideshowActive 
                      ? "bg-amber-500 text-stone-950 hover:bg-amber-600 border-amber-600" 
                      : "bg-white text-stone-800 hover:bg-stone-50 border-[#1a1a1a]"
                  }`}
                  title="Play/Pause Slideshow (Spacebar)"
                >
                  {slideshowActive ? <Pause size={14} className="stroke-[2.5px]" /> : <Play size={14} className="stroke-[2.5px]" />}
                  <span>{slideshowActive ? "Pause" : "Play"}</span>
                </button>

                {/* Direction switcher */}
                <div className="flex items-center gap-0.5 bg-stone-200/60 p-0.5 rounded-lg border border-stone-300">
                  <button
                    onClick={() => setSlideshowDirection('forward')}
                    className={`p-1.5 rounded-md text-[10px] font-bold cursor-pointer transition-all flex items-center gap-1 ${
                      slideshowDirection === 'forward' 
                        ? "bg-[#1a1a1a] text-white" 
                        : "text-stone-600 hover:text-stone-900"
                    }`}
                  >
                    <ArrowRight size={11} />
                    <span>Forward</span>
                  </button>
                  <button
                    onClick={() => setSlideshowDirection('reverse')}
                    className={`p-1.5 rounded-md text-[10px] font-bold cursor-pointer transition-all flex items-center gap-1 ${
                      slideshowDirection === 'reverse' 
                        ? "bg-[#1a1a1a] text-white" 
                        : "text-stone-600 hover:text-stone-900"
                    }`}
                  >
                    <ArrowLeft size={11} />
                    <span>Reverse</span>
                  </button>
                  <button
                    onClick={() => setSlideshowDirection('random1')}
                    className={`p-1.5 rounded-md text-[10px] font-bold cursor-pointer transition-all flex items-center gap-1 ${
                      slideshowDirection === 'random1' 
                        ? "bg-[#1a1a1a] text-white" 
                        : "text-stone-600 hover:text-stone-900"
                    }`}
                  >
                    <Shuffle size={11} />
                    <span>Random 1</span>
                  </button>
                  <button
                    onClick={() => setSlideshowDirection('random2')}
                    className={`p-1.5 rounded-md text-[10px] font-bold cursor-pointer transition-all flex items-center gap-1 ${
                      slideshowDirection === 'random2' 
                        ? "bg-[#1a1a1a] text-white" 
                        : "text-stone-600 hover:text-stone-900"
                    }`}
                  >
                    <Repeat size={11} />
                    <span>Random 2</span>
                  </button>
                </div>

                {/* Speeds */}
                <div className="flex items-center gap-1 bg-stone-200/60 p-0.5 rounded-lg border border-stone-300">
                  {[1000, 2000, 3000, 5000].map(speed => (
                    <button
                      key={speed}
                      onClick={() => setSlideshowSpeed(speed)}
                      className={`px-2 py-0.5 rounded text-[10px] font-mono font-bold cursor-pointer transition-all ${
                        slideshowSpeed === speed
                          ? "bg-[#1a1a1a] text-white"
                          : "text-stone-600 hover:text-stone-900"
                      }`}
                    >
                      {speed / 1000}s
                    </button>
                  ))}
                </div>
              </div>
            </div>

          </motion.div>
        )}
      </AnimatePresence>

      {/* EXPORT POSTER PRINT PREVIEW OVERLAY */}
      <AnimatePresence>
        {isExporting && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            className="fixed inset-0 bg-stone-900/98 backdrop-blur-md z-[10000] overflow-y-auto p-4 md:p-8 flex flex-col items-center"
          >
            {/* Header / Actions */}
            <div className="w-full max-w-4xl flex flex-col gap-3 mb-6 bg-white/10 backdrop-blur-md p-5 rounded-xl border border-white/10 text-white flex-shrink-0 no-print">
              <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
                <div>
                  <h3 className="text-sm font-bold font-mono uppercase tracking-widest text-amber-400">Export & Print Preview</h3>
                  <p className="text-[11px] text-stone-300">This layout is optimized for high-quality printing or saving as a PDF file.</p>
                </div>
                <div className="flex items-center gap-3">
                  <button
                    onClick={handlePrint}
                    className="px-4 py-2 bg-emerald-600 hover:bg-emerald-700 text-white font-mono font-bold text-xs rounded-lg flex items-center gap-2 shadow-md cursor-pointer transition-all"
                  >
                    <Printer size={14} />
                    <span>Print or Save PDF</span>
                  </button>
                  <button
                    onClick={() => setIsExporting(false)}
                    className="px-4 py-2 bg-stone-800 hover:bg-stone-700 text-stone-200 font-mono font-bold text-xs rounded-lg flex items-center gap-2 border border-stone-700 cursor-pointer transition-all"
                  >
                    <X size={14} />
                    <span>Close Preview</span>
                  </button>
                </div>
              </div>

              {/* Iframe sandbox notice */}
              {isInIframe && (
                <div className="mt-1 p-3 bg-amber-500/10 border border-amber-500/20 rounded-lg text-xs flex items-start gap-2.5 text-amber-200">
                  <span className="text-sm">⚠️</span>
                  <div className="space-y-1">
                    <p className="font-semibold text-[10px] font-mono uppercase tracking-wider text-amber-400">Sandboxed Preview Frame Detected</p>
                    <p className="leading-relaxed text-stone-300 text-[10px]">
                      Your browser's security restrictions blocks print dialogs inside standard frames. 
                      To save your poster as a PDF: click the <strong className="text-white font-semibold">"Open in new tab"</strong> button at the top-right of the screen, and print from there!
                    </p>
                  </div>
                </div>
              )}

              {printError && (
                <div className="mt-1 p-3 bg-red-500/10 border border-red-500/20 rounded-lg text-xs flex items-start gap-2.5 text-red-200">
                  <span className="text-sm">❌</span>
                  <div className="space-y-1">
                    <p className="font-semibold text-[10px] font-mono uppercase tracking-wider text-red-400">Print Interrupted</p>
                    <p className="leading-relaxed text-stone-300 text-[10px]">{printError}</p>
                  </div>
                </div>
              )}
            </div>

            {/* Printable Poster Container */}
            <div 
              className={`w-full max-w-4xl rounded-2xl p-6 sm:p-8 shadow-2xl relative border border-[#e4ded5]/60 print-m-0 print-p-0 print-shadow-none overflow-hidden ${getBgClass(collageConfig.background)} flex flex-col justify-between`}
              id="print-poster-root"
              style={{
                aspectRatio: "16 / 12.2",
              }}
            >
              {/* Optional Active Effects */}
              {renderActiveEffects(collageConfig.activeEffects)}

              {/* Style injections for beautiful high-fidelity browser printing */}
              <style>{`
                @media print {
                  body * {
                    visibility: hidden;
                  }
                  #print-poster-root, #print-poster-root * {
                    visibility: visible;
                  }
                  #print-poster-root {
                    position: absolute;
                    left: 0;
                    top: 0;
                    width: 100% !important;
                    max-width: 100% !important;
                    height: auto !important;
                    padding: 30px !important;
                    box-shadow: none !important;
                    border: none !important;
                    overflow: hidden !important;
                    background: ${
                      collageConfig.background === "minimal" ? "#faf6ee" :
                      collageConfig.background === "slate" ? "#0f172a" :
                      collageConfig.background === "dark" ? "#0a0a0a" :
                      collageConfig.background === "sunset" ? "linear-gradient(135deg, #f59e0b, #ec4899, #4f46e5)" :
                      collageConfig.background === "beach" ? "linear-gradient(135deg, #7dd3fc, #2dd4bf, #fef3c7)" :
                      collageConfig.background === "space" ? "linear-gradient(135deg, #3b0764, #0f172a)" :
                      collageConfig.background === "vibrant" ? "linear-gradient(135deg, #c084fc, #f43f5e, #fbbf24)" : "#faf6ee"
                    } !important;
                    -webkit-print-color-adjust: exact !important;
                    print-color-adjust: exact !important;
                  }
                  #print-poster-root .collage-canvas-container {
                    overflow: hidden !important;
                  }
                  .no-print {
                    display: none !important;
                  }
                }
              `}</style>

              {/* Title Header */}
              {collageConfig.title && (
                <div className="text-center mb-6 border-b border-stone-200/20 pb-4 relative z-[2000]">
                  <h1 className={`text-2xl font-bold font-mono tracking-wider ${isLightBg ? "text-stone-900" : "text-white"}`}>
                    {collageConfig.title}
                  </h1>
                </div>
              )}

              {/* Display Collage Items */}
              {collageConfig.template === "stacked" ? (
                <div className="relative w-full aspect-[16/10] select-none overflow-hidden collage-canvas-container">
                  {collageCars.map((car, idx) => {
                    const layout = carLayouts[car.id] || getInitialLayoutForCar(car.id, idx);
                    if (layout.visible === false) return null;

                    const parsed = parseFilename(car.filename);
                    const cardStyle = {
                      position: "absolute" as const,
                      left: `${layout.x}%`,
                      top: `${layout.y}%`,
                      width: `${(layout.width / 600) * 100}%`,
                      transform: `translate(-50%, -50%) rotate(${layout.rotation}deg)`,
                      zIndex: layout.zIndex,
                    };
                    return (
                      <div
                        key={car.id}
                        style={cardStyle}
                        className={`absolute border overflow-hidden ${getCardRadiusClass(collageConfig.cardRadius)} ${getShadowClass(collageConfig.shadowSize)} ${isLightBg ? "bg-white border-[#e4ded5]" : "bg-slate-900 border-slate-800"}`}
                      >
                        <div className="relative bg-stone-100 overflow-hidden aspect-video">
                          <img src={car.imageUrl} className="h-full w-full object-cover" alt="Car" />
                          {collageConfig.showScaleTags && (
                            <span className={`absolute bottom-1.5 right-1.5 px-1.5 py-0.5 font-mono text-[9px] font-semibold rounded border ${isLightBg ? "bg-stone-50/90 text-stone-900 border-[#e4ded5]" : "bg-slate-950/80 text-emerald-400 border-slate-800"}`}>
                              {car.scale}
                            </span>
                          )}
                        </div>
                        {collageConfig.showLabels && (
                          <div className={`p-2 ${getTextAlignmentClass(collageConfig.textAlignment)}`}>
                            <p className={`font-mono text-[10px] font-bold truncate ${isLightBg ? "text-stone-900" : "text-white"}`}>
                              {car.filename}
                            </p>
                            <p className="text-[9px] text-stone-500 font-sans mt-0.5 truncate">{parsed.fullName}</p>
                          </div>
                        )}
                      </div>
                    );
                  })}
                </div>
              ) : collageConfig.template === "grid" ? (
                <div 
                  className="grid grid-cols-2 md:grid-cols-3 gap-6"
                  style={{ gap: `${collageConfig.spacing}px` }}
                >
                  {collageCars.map((car, idx) => {
                    const layout = carLayouts[car.id];
                    if (layout && layout.visible === false) return null;

                    const parsed = parseFilename(car.filename);
                    return (
                      <div
                        key={car.id}
                        className={`overflow-hidden border ${getCardRadiusClass(collageConfig.cardRadius)} ${getShadowClass(collageConfig.shadowSize)} ${isLightBg ? "bg-white border-[#e4ded5]" : "bg-slate-900 border-slate-800"}`}
                      >
                        <div className={`${getPhotoRatioClass(collageConfig.photoRatio, "aspect-video")} bg-stone-100 overflow-hidden relative`}>
                          <img src={car.imageUrl} className={`h-full w-full ${collageConfig.photoFit === "contain" ? "object-contain bg-stone-100" : "object-cover"}`} alt="Car" />
                          {collageConfig.showScaleTags && (
                            <span className={`absolute bottom-2 right-2 px-2 py-0.5 font-mono text-[10px] font-semibold rounded border ${isLightBg ? "bg-stone-50/90 text-stone-900 border-[#e4ded5]" : "bg-slate-950/80 text-emerald-400 border-slate-800"}`}>
                              {car.scale}
                            </span>
                          )}
                        </div>
                        {collageConfig.showLabels && (
                          <div className={`p-3 ${getTextAlignmentClass(collageConfig.textAlignment)}`}>
                            <p className={`font-mono text-xs truncate ${isLightBg ? "text-stone-900 font-bold" : "text-white"}`}>
                              {car.filename}
                            </p>
                            <p className="text-[10px] text-stone-500 font-sans mt-0.5">{parsed.fullName} • <span className={`font-bold font-mono text-[10px] ${isLightBg ? "text-stone-700" : "text-emerald-500"}`}>{car.brand}</span></p>
                          </div>
                        )}
                      </div>
                    );
                  })}
                </div>
              ) : collageConfig.template === "strip" ? (
                <div className="space-y-6">
                  <div className={`flex justify-around select-none ${isLightBg ? "text-stone-300" : "text-slate-800"}`}>
                    {Array.from({ length: 16 }).map((_, i) => (
                      <div key={i} className="h-3 w-5 bg-current rounded-sm" />
                    ))}
                  </div>
                  <div className="grid grid-cols-2 gap-4">
                    {collageCars.map((car, idx) => {
                      const layout = carLayouts[car.id];
                      if (layout && layout.visible === false) return null;

                      const parsed = parseFilename(car.filename);
                      return (
                        <div 
                          key={car.id}
                          className={`overflow-hidden border ${getCardRadiusClass(collageConfig.cardRadius)} ${getShadowClass(collageConfig.shadowSize)} ${isLightBg ? "bg-white border-[#e4ded5]" : "bg-slate-900 border-slate-800"}`}
                        >
                          <div className={`${getPhotoRatioClass(collageConfig.photoRatio, "aspect-video")} relative bg-stone-100`}>
                            <img src={car.imageUrl} className={`h-full w-full ${collageConfig.photoFit === "contain" ? "object-contain bg-stone-100" : "object-cover"}`} alt="Car" />
                          </div>
                          {collageConfig.showLabels && (
                            <div className={`p-3 ${getTextAlignmentClass(collageConfig.textAlignment)}`}>
                              <p className={`font-mono text-xs truncate ${isLightBg ? "text-stone-900 font-bold" : "text-emerald-400"}`}>{car.filename}</p>
                              <p className="text-[10px] text-stone-500 mt-1">
                                {parsed.fullName}{collageConfig.showScaleTags ? ` (${car.scale})` : ""}
                              </p>
                            </div>
                          )}
                        </div>
                      );
                    })}
                  </div>
                  <div className={`flex justify-around select-none ${isLightBg ? "text-stone-300" : "text-slate-800"}`}>
                    {Array.from({ length: 16 }).map((_, i) => (
                      <div key={i} className="h-3 w-5 bg-current rounded-sm" />
                    ))}
                  </div>
                </div>
              ) : collageConfig.template === "polaroid" ? (
                <div className="flex flex-wrap justify-center gap-6 py-4">
                  {collageCars.map((car, idx) => {
                    const layout = carLayouts[car.id];
                    if (layout && layout.visible === false) return null;

                    const parsed = parseFilename(car.filename);
                    const rotations = ["rotate-1", "-rotate-1", "rotate-2", "-rotate-2", "rotate-3", "-rotate-3"];
                    const randomRotation = rotations[idx % rotations.length];
                    return (
                      <div
                        key={car.id}
                        className={`text-stone-900 bg-white p-4 pb-6 border border-stone-200 max-w-[220px] ${randomRotation} ${getCardRadiusClass(collageConfig.cardRadius)} ${getShadowClass(collageConfig.shadowSize)}`}
                      >
                        <div className={`relative bg-stone-50 overflow-hidden border border-stone-200 ${getPhotoRatioClass(collageConfig.photoRatio, "aspect-square")}`}>
                          <img src={car.imageUrl} className={`h-full w-full ${collageConfig.photoFit === "contain" ? "object-contain bg-stone-100" : "object-cover"}`} alt="Polaroid car" />
                        </div>
                        {collageConfig.showLabels && (
                          <div className={`mt-4 ${getTextAlignmentClass(collageConfig.textAlignment)}`}>
                            <p className="font-mono text-[10px] font-bold tracking-tight text-stone-850 truncate max-w-full">
                              {car.filename}
                            </p>
                            <p className="text-[10px] text-stone-500 font-medium italic mt-1">
                              {parsed.fullName}
                            </p>
                          </div>
                        )}
                        {collageConfig.showScaleTags && (
                          <div className="mt-2 flex justify-between text-[10px] text-stone-800 font-bold font-mono">
                            <span>{car.scale}</span>
                            <span>{car.brand}</span>
                          </div>
                        )}
                      </div>
                    );
                  })}
                </div>
              ) : (
                <div className="columns-2 gap-4 space-y-4">
                  {collageCars.map((car, idx) => {
                    const layout = carLayouts[car.id];
                    if (layout && layout.visible === false) return null;

                    const parsed = parseFilename(car.filename);
                    return (
                      <div 
                        key={car.id} 
                        className={`break-inside-avoid overflow-hidden border ${getCardRadiusClass(collageConfig.cardRadius)} ${getShadowClass(collageConfig.shadowSize)} ${isLightBg ? "bg-white border-[#e4ded5]" : "bg-slate-900 border-slate-800"}`}
                      >
                        <div className={`relative overflow-hidden bg-stone-100 ${getPhotoRatioClass(collageConfig.photoRatio, "aspect-auto")}`}>
                          <img src={car.imageUrl} className={`w-full h-auto ${collageConfig.photoFit === "contain" ? "object-contain bg-stone-100" : "object-cover"}`} alt="Masonry car" />
                        </div>
                        <div className={`p-3 ${getTextAlignmentClass(collageConfig.textAlignment)}`}>
                          <p className={`font-mono text-xs truncate ${isLightBg ? "text-stone-900 font-bold" : "text-white"}`}>
                            {car.filename}
                          </p>
                          <p className="text-[10px] text-stone-500 font-sans mt-0.5">{parsed.fullName}</p>
                        </div>
                      </div>
                    );
                  })}
                </div>
              )}

              {/* Poster Footer Badging */}
              <div className="mt-4 pt-3 sm:mt-12 sm:pt-6 border-t border-stone-200/20 flex flex-wrap items-center justify-between gap-4 text-[10px] font-mono tracking-widest uppercase">
                <span className={isLightBg ? "text-stone-500" : "text-stone-400"}>
                  Registered Collector Poster Presentation
                </span>
                <span className={`font-bold ${isLightBg ? "text-stone-900" : "text-amber-400"}`}>
                  Diecast Blueprint Studio
                </span>
              </div>
            </div>
          </motion.div>
        )}
      </AnimatePresence>

    </div>
  );
}
