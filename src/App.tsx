import React, { useState, useEffect, useMemo, useRef } from "react";
import { 
  Database, Star, Award, Layers, HelpCircle, AlertCircle, Share2, Sparkles, Tag, Pencil, Upload, Trash, Save, Undo2, Redo2, CheckCircle, Info, X, Palette, Download
} from "lucide-react";
import { motion, AnimatePresence } from "motion/react";
import { DiecastCar, FilterState, ScaleType, AINuggets } from "./types";
import { INITIAL_CARS, parseFilename } from "./data/cars";
import LeftPanel from "./components/LeftPanel";
import MiddlePanel from "./components/MiddlePanel";
import RightPanel from "./components/RightPanel";
import { getPhoto, clearAllPhotos, deletePhoto } from "./utils/photoStorage";

const LOCAL_STORAGE_KEY = "diecast_catalogue_cars";

export default function App() {
  const [cars, setCars] = useState<DiecastCar[]>([]);
  const [selectedIds, setSelectedIds] = useState<string[]>([]);
  const [activeCarId, setActiveCarId] = useState<string | null>(null);
  
  // Custom non-blocking modal confirmation dialog
  const [modalConfig, setModalConfig] = useState<{
    title: string;
    message: string;
    confirmLabel: string;
    isDestructive?: boolean;
    onConfirm: () => void | Promise<void>;
  } | null>(null);

  // Custom non-blocking toast notifications
  const [toast, setToast] = useState<{
    message: string;
    type: "success" | "error" | "info";
  } | null>(null);
  
  // Undo/Redo Stacks
  const [history, setHistory] = useState<DiecastCar[][]>([]);
  const [redoStack, setRedoStack] = useState<DiecastCar[][]>([]);
  const [showSavedFeedback, setShowSavedFeedback] = useState(false);

  // App Title / Naming state
  const [appName, setAppName] = useState(() => {
    return localStorage.getItem("diecast_app_name") || "My Collection";
  });
  const [isEditingAppName, setIsEditingAppName] = useState(false);
  const [tempAppName, setTempAppName] = useState("");

  // App Custom Icon state
  const [appIcon, setAppIcon] = useState<string | null>(() => {
    return localStorage.getItem("diecast_app_icon") || null;
  });
  const iconInputRef = useRef<HTMLInputElement>(null);

  const handleIconChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (file) {
      const reader = new FileReader();
      reader.onloadend = () => {
        const base64String = reader.result as string;
        setAppIcon(base64String);
        localStorage.setItem("diecast_app_icon", base64String);
      };
      reader.readAsDataURL(file);
    }
  };

  const handleResetIcon = (e: React.MouseEvent) => {
    e.stopPropagation();
    setAppIcon(null);
    localStorage.removeItem("diecast_app_icon");
    if (iconInputRef.current) {
      iconInputRef.current.value = "";
    }
  };
  
  // Theme state: "warm", "navy", "tomica"
  const [theme, setTheme] = useState<"warm" | "navy" | "tomica">(() => {
    const saved = localStorage.getItem("app_theme");
    if (saved === "current" || saved === "monochrome" || !saved) return "warm";
    return saved as "warm" | "navy" | "tomica";
  });

  useEffect(() => {
    document.body.className = `theme-${theme}`;
  }, [theme]);

  // Centralized AI Nuggets State
  const [nuggetCache, setNuggetCache] = useState<Record<string, AINuggets>>({});
  const [nuggetsLoading, setNuggetsLoading] = useState(false);
  const [nuggetsError, setNuggetsError] = useState<string | null>(null);
  
  // Filter states
  const [filters, setFilters] = useState<FilterState>({
    searchQuery: "",
    scales: [],
    brands: [],
    tags: [],
    conditions: [],
    onlyFavorites: false
  });

  // Load cars on startup and recreate blob URLs from IndexedDB
  useEffect(() => {
    const initCars = async () => {
      try {
        const stored = localStorage.getItem(LOCAL_STORAGE_KEY);
        if (stored) {
          const parsed = JSON.parse(stored);
          if (Array.isArray(parsed) && parsed.length > 0) {
            // Reconstruct active blob URLs from IndexedDB
            const restored = await Promise.all(parsed.map(async (car: DiecastCar) => {
              let cleanBrand = car.brand;
              if (/^hot\s*wheels$/i.test(car.brand) || /^hotwheels$/i.test(car.brand)) {
                cleanBrand = "Kyosho";
                if (/subaru|nissan|toyota|honda|mazda|mitsubishi|lexus|suzuki|daihatsu/i.test(car.filename)) {
                  cleanBrand = "Tomica";
                }
              }

              // Retrieve the actual photo Blob from IndexedDB
              const blob = await getPhoto(car.id);
              let imageUrl = car.imageUrl;
              if (blob) {
                imageUrl = URL.createObjectURL(blob);
              }

              return { ...car, brand: cleanBrand, imageUrl };
            }));

            setCars(restored);
            setActiveCarId(restored[0].id);
            setSelectedIds([restored[0].id]);
            return;
          }
        }
      } catch (e) {
        console.error("Failed to parse local storage cars:", e);
      }
      
      // Fallback to initial seed data
      setCars(INITIAL_CARS);
      if (INITIAL_CARS.length > 0) {
        setActiveCarId(INITIAL_CARS[0].id);
        setSelectedIds([INITIAL_CARS[0].id]);
      }
    };

    initCars();
  }, []);

  // Sync cars to local storage
  const saveToLocalStorage = (updatedCars: DiecastCar[]) => {
    try {
      localStorage.setItem(LOCAL_STORAGE_KEY, JSON.stringify(updatedCars));
    } catch (e) {
      console.error("Failed to save cars to local storage:", e);
    }
  };

  // Find active car object
  const activeCar = useMemo(() => {
    return cars.find(c => c.id === activeCarId) || null;
  }, [cars, activeCarId]);

  // Auto-fetch nuggets from Gemini when the active car changes
  useEffect(() => {
    if (!activeCar) return;
    const filename = activeCar.filename;
    if (nuggetCache[filename]) {
      setNuggetsError(null);
      return;
    }

    const fetchNuggets = async () => {
      setNuggetsLoading(true);
      setNuggetsError(null);
      try {
        const parsed = parseFilename(filename);
        const response = await fetch("/api/car-nuggets", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ filename, carName: parsed.fullName }),
        });

        if (!response.ok) {
          throw new Error("Server failed to generate model history.");
        }

        const data: AINuggets = await response.json();
        setNuggetCache(prev => ({
          ...prev,
          [filename]: data
        }));
      } catch (err: any) {
        console.error("Gemini nuggets fetch failed:", err);
        setNuggetsError(err.message || "Failed to load vehicle history.");
      } finally {
        setNuggetsLoading(false);
      }
    };

    fetchNuggets();
  }, [activeCar, activeCar?.filename]);

  const handleRetryFetchNuggets = () => {
    if (!activeCar) return;
    const filename = activeCar.filename;
    setNuggetCache(prev => {
      const copy = { ...prev };
      delete copy[filename];
      return copy;
    });
    setNuggetsError(null);
  };

  // Filtered cars calculation
  const filteredCars = useMemo(() => {
    return cars.filter(car => {
      const query = filters.searchQuery.toLowerCase();
      const parsed = parseFilename(car.filename);
      const matchesSearch = car.filename.toLowerCase().includes(query) ||
        parsed.fullName.toLowerCase().includes(query) ||
        car.brand.toLowerCase().includes(query) ||
        car.tags.some(t => t.toLowerCase().includes(query)) ||
        (car.category && car.category.toLowerCase().includes(query)) ||
        car.notes.toLowerCase().includes(query);

      const matchesScale = filters.scales.length === 0 || filters.scales.includes(car.scale);
      const matchesBrand = filters.brands.length === 0 || filters.brands.includes(car.brand);
      const matchesTags = filters.tags.length === 0 || car.tags.some(t => filters.tags.includes(t));
      const matchesCategories = !filters.categories || filters.categories.length === 0 || (car.category && filters.categories.includes(car.category));
      const matchesFavorite = !filters.onlyFavorites || car.isFavorite;

      return matchesSearch && matchesScale && matchesBrand && matchesTags && matchesCategories && matchesFavorite;
    });
  }, [cars, filters]);

  // Find all selected car objects
  const selectedCars = useMemo(() => {
    return cars.filter(c => selectedIds.includes(c.id));
  }, [cars, selectedIds]);

  // Portfolio Statistics Engine
  const stats = useMemo(() => {
    const totalCount = cars.length;
    const favoriteCount = cars.filter(c => c.isFavorite).length;
    
    // Unique brands count
    const uniqueBrands = new Set(cars.map(c => c.brand.trim()).filter(Boolean)).size;

    // Scale counts
    const scalesBreakdown = cars.reduce((acc, car) => {
      acc[car.scale] = (acc[car.scale] || 0) + 1;
      return acc;
    }, {} as Record<string, number>);

    return {
      totalCount,
      favoriteCount,
      uniqueBrands,
      scalesBreakdown
    };
  }, [cars]);

  // Handle Selection & Interaction Flow
  const handleCarSelect = (id: string, isMultiToggle = false) => {
    if (isMultiToggle) {
      // Toggle selection list for checkbox clicks
      setSelectedIds(prev => {
        if (prev.includes(id)) {
          const filtered = prev.filter(item => item !== id);
          // If we emptied selection, reset active car
          if (filtered.length === 1) {
            setActiveCarId(filtered[0]);
          }
          return filtered;
        } else {
          const next = [...prev, id];
          // Make the last checked one the active inspected car
          setActiveCarId(id);
          return next;
        }
      });
    } else {
      // Direct click on card: inspect single car & make it sole selection
      setActiveCarId(id);
      setSelectedIds([id]);
    }
  };

  // Bulk Actions
  const handleSelectAll = (ids: string[]) => {
    setSelectedIds(ids);
  };

  const handleClearSelection = () => {
    setSelectedIds([]);
    setActiveCarId(null);
  };

  // Push state to undo history
  const pushToHistory = (currentCars: DiecastCar[]) => {
    setHistory(prev => {
      const next = [...prev, currentCars];
      if (next.length > 20) {
        return next.slice(next.length - 20); // limit to 20 steps
      }
      return next;
    });
    setRedoStack([]); // clear redo stack on new actions
  };

  const handleUndo = () => {
    if (history.length === 0) return;
    const previous = history[history.length - 1];
    setHistory(prev => prev.slice(0, prev.length - 1));
    setRedoStack(prev => [cars, ...prev]);
    setCars(previous);
    saveToLocalStorage(previous);
  };

  const handleRedo = () => {
    if (redoStack.length === 0) return;
    const next = redoStack[0];
    setRedoStack(prev => prev.slice(1));
    setHistory(prev => [...prev, cars]);
    setCars(next);
    saveToLocalStorage(next);
  };

  const handleSaveState = () => {
    try {
      localStorage.setItem(LOCAL_STORAGE_KEY, JSON.stringify(cars));
      localStorage.setItem("diecast_app_name", appName);
      if (appIcon) {
        localStorage.setItem("diecast_app_icon", appIcon);
      }
      setShowSavedFeedback(true);
      setTimeout(() => {
        setShowSavedFeedback(false);
      }, 2000);
    } catch (e) {
      console.error("Manual save failed:", e);
      setToast({
        message: "Failed to save state to local storage.",
        type: "error"
      });
      setTimeout(() => setToast(null), 4000);
    }
  };

  // Mutator Actions
  const handleAddCar = (newCar: DiecastCar) => {
    pushToHistory(cars);
    const updated = [newCar, ...cars];
    setCars(updated);
    saveToLocalStorage(updated);

    // Auto-select and inspect the newly added car
    setActiveCarId(newCar.id);
    setSelectedIds([newCar.id]);
  };

  const handleAddCars = (newCars: DiecastCar[]) => {
    pushToHistory(cars);
    const updated = [...newCars, ...cars];
    setCars(updated);
    saveToLocalStorage(updated);

    if (newCars.length > 0) {
      setActiveCarId(newCars[0].id);
      setSelectedIds([newCars[0].id]);
    }
  };

  const handleUpdateAllCars = (updatedCars: DiecastCar[]) => {
    pushToHistory(cars);
    setCars(updatedCars);
    saveToLocalStorage(updatedCars);
    if (activeCarId && updatedCars.some(c => c.id === activeCarId)) {
      // Keep existing active selection
    } else if (updatedCars.length > 0) {
      setActiveCarId(updatedCars[0].id);
      setSelectedIds([updatedCars[0].id]);
    } else {
      setActiveCarId(null);
      setSelectedIds([]);
    }
  };

  const handleDeleteCar = (id: string) => {
    pushToHistory(cars);
    const updated = cars.filter(c => c.id !== id);
    setCars(updated);
    saveToLocalStorage(updated);

    // Cleanup active & selection targets
    if (activeCarId === id) {
      const nextActive = updated[0]?.id || null;
      setActiveCarId(nextActive);
      setSelectedIds(nextActive ? [nextActive] : []);
    } else {
      setSelectedIds(prev => prev.filter(item => item !== id));
    }
  };

  const handleToggleFavorite = (id: string) => {
    pushToHistory(cars);
    const updated = cars.map(car => 
      car.id === id ? { ...car, isFavorite: !car.isFavorite } : car
    );
    setCars(updated);
    saveToLocalStorage(updated);
  };

  const handleUpdateTags = (id: string, tags: string[]) => {
    pushToHistory(cars);
    const updated = cars.map(car => 
      car.id === id ? { ...car, tags } : car
    );
    setCars(updated);
    saveToLocalStorage(updated);
  };

  const handleAddTagToCar = (carId: string, tag: string) => {
    pushToHistory(cars);

    let updateFunc: (car: DiecastCar) => DiecastCar;

    if (tag.startsWith("category:")) {
      const val = tag.substring("category:".length);
      updateFunc = (car) => ({ ...car, category: val });
    } else if (tag.startsWith("brand:")) {
      const val = tag.substring("brand:".length);
      updateFunc = (car) => ({ ...car, brand: val });
    } else if (tag.startsWith("scale:")) {
      const val = tag.substring("scale:".length) as ScaleType;
      updateFunc = (car) => ({ ...car, scale: val });
    } else if (tag.startsWith("tag:")) {
      const val = tag.substring("tag:".length);
      const splitTags = val.split(/[,;]+/).map(t => t.trim().toLowerCase()).filter(Boolean);
      updateFunc = (car) => ({
        ...car,
        tags: Array.from(new Set([...car.tags, ...splitTags]))
      });
    } else {
      // Fallback/Legacy
      const trimmed = tag.trim();
      const isScale = ["1:18", "1:24", "1:43", "1:64", "1:87"].includes(trimmed);
      if (isScale) {
        updateFunc = (car) => ({ ...car, scale: trimmed as ScaleType });
      } else {
        const splitTags = trimmed.split(/[,;]+/).map(t => t.trim().toLowerCase()).filter(Boolean);
        updateFunc = (car) => ({
          ...car,
          tags: Array.from(new Set([...car.tags, ...splitTags]))
        });
      }
    }

    // If the target car is part of the current multi-selection, apply to all selected cars!
    const targetIds = selectedIds.includes(carId) ? selectedIds : [carId];
    const updated = cars.map(car => {
      if (targetIds.includes(car.id)) {
        return updateFunc(car);
      }
      return car;
    });

    setCars(updated);
    saveToLocalStorage(updated);
  };

  const handleUpdateNotes = (id: string, notes: string) => {
    pushToHistory(cars);
    const updated = cars.map(car => 
      car.id === id ? { ...car, notes } : car
    );
    setCars(updated);
    saveToLocalStorage(updated);
  };

  const handleUpdateCarDetails = (id: string, updates: Partial<DiecastCar>) => {
    pushToHistory(cars);
    const updated = cars.map(car => 
      car.id === id ? { ...car, ...updates } : car
    );
    setCars(updated);
    saveToLocalStorage(updated);
  };

  // Random Selection Roller
  const handleRandomSelect = (count: number) => {
    if (cars.length === 0) return;
    
    const shuffled = [...cars].sort(() => 0.5 - Math.random());
    const rolled = shuffled.slice(0, Math.min(count, cars.length));
    const rolledIds = rolled.map(c => c.id);
    
    setSelectedIds(rolledIds);
    if (rolledIds.length > 0) {
      setActiveCarId(rolledIds[0]);
    }
  };

  // Quick action: share catalog link copy
  const handleShareLink = () => {
    const url = window.location.href;
    navigator.clipboard.writeText(url);
    setToast({
      message: "Shareable catalogue link copied to clipboard! Share it with fellow collectors.",
      type: "success"
    });
    setTimeout(() => setToast(null), 4000);
  };

  // Clear entire collection or selected photos
  const handleUnloadAllPhotos = async (idsToUnload?: string[]) => {
    const targets = idsToUnload && idsToUnload.length > 0 ? idsToUnload : [];
    
    if (targets.length > 0) {
      setModalConfig({
        title: "Unload Selected Photos",
        message: `Are you sure you want to unload the ${targets.length} selected photos? This will remove them from your catalog.`,
        confirmLabel: `Unload Selected (${targets.length})`,
        isDestructive: true,
        onConfirm: async () => {
          pushToHistory(cars);
          const updated = cars.filter(c => !targets.includes(c.id));
          setCars(updated);
          saveToLocalStorage(updated);
          
          // Remove from IndexedDB
          for (const id of targets) {
            await deletePhoto(id);
          }
          
          setSelectedIds([]);
          if (activeCarId && targets.includes(activeCarId)) {
            setActiveCarId(updated[0]?.id || null);
          }
          setModalConfig(null);
          setToast({
            message: `Successfully unloaded ${targets.length} photo${targets.length > 1 ? "s" : ""}.`,
            type: "success"
          });
          setTimeout(() => setToast(null), 3000);
        }
      });
    } else {
      setModalConfig({
        title: "Unload Entire Collection",
        message: "Are you sure you want to unload all photos and clear your entire collection? This will remove all files from your catalog.",
        confirmLabel: "Unload All Photos",
        isDestructive: true,
        onConfirm: async () => {
          pushToHistory(cars);
          setCars([]);
          saveToLocalStorage([]);
          setSelectedIds([]);
          setActiveCarId(null);
          await clearAllPhotos();
          setModalConfig(null);
          setToast({
            message: "Successfully cleared your entire collection.",
            type: "success"
          });
          setTimeout(() => setToast(null), 3000);
        }
      });
    }
  };

  return (
    <div className={`flex flex-col h-screen bg-[#faf6ee] text-[#1a1a1a] overflow-hidden font-sans select-none theme-${theme}`}>
      
      {/* Dynamic Top Header Bar */}
      <header className="h-[60px] border-b border-[#e4ded5] bg-white px-6 flex items-center justify-between flex-shrink-0 z-30">
        
        {/* Branding Logo */}
        <div className="flex items-center gap-3">
          <div 
            onClick={() => iconInputRef.current?.click()}
            className="p-1.5 bg-[#1a1a1a] rounded-xl shadow-sm border border-[#2b2b2b] relative cursor-pointer group flex-shrink-0 flex items-center justify-center w-9 h-9 hover:border-stone-400 transition-all"
            title="Click to upload custom app icon"
          >
            {appIcon ? (
              <img 
                src={appIcon} 
                alt="App icon" 
                className="w-5.5 h-5.5 object-cover rounded-md"
                referrerPolicy="no-referrer"
              />
            ) : (
              <Database size={18} className="text-[#faf6ee] stroke-[2.5]" />
            )}
            
            {/* Minimal hover overlay */}
            <div className="absolute inset-0 bg-black/50 rounded-xl opacity-0 group-hover:opacity-100 flex items-center justify-center transition-opacity">
              <Upload size={10} className="text-white" />
            </div>
          </div>

          <input 
            type="file" 
            ref={iconInputRef} 
            onChange={handleIconChange} 
            accept="image/*" 
            className="hidden" 
          />

          <div className="flex items-center gap-3">
            {isEditingAppName ? (
              <div className="flex items-center gap-1.5 py-0.5">
                <input
                  type="text"
                  value={tempAppName}
                  onChange={(e) => setTempAppName(e.target.value)}
                  onBlur={() => {
                    const trimmed = tempAppName.trim();
                    if (trimmed) {
                      setAppName(trimmed);
                      localStorage.setItem("diecast_app_name", trimmed);
                    }
                    setIsEditingAppName(false);
                  }}
                  onKeyDown={(e) => {
                    if (e.key === "Enter") {
                      const trimmed = tempAppName.trim();
                      if (trimmed) {
                        setAppName(trimmed);
                        localStorage.setItem("diecast_app_name", trimmed);
                      }
                      setIsEditingAppName(false);
                    } else if (e.key === "Escape") {
                      setTempAppName(appName);
                      setIsEditingAppName(false);
                    }
                  }}
                  autoFocus
                  className="bg-white border border-[#1a1a1a] rounded px-2 py-0.5 text-xs font-bold tracking-wider font-mono text-[#1a1a1a] focus:outline-none w-44"
                />
              </div>
            ) : (
              <h1 
                onClick={() => {
                  setTempAppName(appName);
                  setIsEditingAppName(true);
                }}
                className="text-sm font-bold tracking-wider font-mono text-[#1a1a1a] flex items-center gap-1.5 cursor-pointer group hover:text-amber-800 transition-colors"
                title="Click to rename app"
              >
                {appName}
                <Pencil size={11} className="opacity-0 group-hover:opacity-100 transition-opacity text-stone-500" />
              </h1>
            )}

            {/* Save Button */}
            <button
              onClick={handleSaveState}
              className={`px-2.5 py-1 text-[11px] font-mono font-bold rounded-lg border transition-all cursor-pointer flex items-center gap-1 shadow-xs ${
                showSavedFeedback 
                  ? "bg-emerald-600 text-white border-emerald-600" 
                  : "bg-white text-stone-700 hover:text-stone-950 border-[#e4ded5] hover:border-stone-400"
              }`}
              title="Save catalog state"
            >
              {showSavedFeedback ? (
                <>
                  <CheckCircle size={12} className="text-white" />
                  <span>Saved!</span>
                </>
              ) : (
                <>
                  <Save size={12} className="text-stone-500 group-hover:text-stone-850" />
                  <span>Save</span>
                </>
              )}
            </button>

            {/* Undo / Redo Actions Stack */}
            <div className="flex items-center bg-white rounded-lg border border-[#e4ded5] p-0.5 shadow-xs">
              <button
                onClick={handleUndo}
                disabled={history.length === 0}
                className={`p-1 rounded cursor-pointer transition-colors ${
                  history.length > 0 
                    ? "text-stone-700 hover:text-stone-950 hover:bg-stone-50" 
                    : "text-stone-300 cursor-not-allowed"
                }`}
                title={`Undo (${history.length}/20 steps)`}
              >
                <Undo2 size={13} />
              </button>
              <button
                onClick={handleRedo}
                disabled={redoStack.length === 0}
                className={`p-1 rounded cursor-pointer transition-colors ${
                  redoStack.length > 0 
                    ? "text-stone-700 hover:text-stone-950 hover:bg-stone-50" 
                    : "text-stone-300 cursor-not-allowed"
                }`}
                title="Redo"
              >
                <Redo2 size={13} />
              </button>
            </div>

            {/* Theme Selector Dropdown */}
            <div className="flex items-center bg-white rounded-lg border border-[#e4ded5] px-2 py-1 shadow-xs gap-1">
              <Palette size={12} className="text-stone-500" />
              <select
                value={theme}
                onChange={(e) => {
                  const newTheme = e.target.value as "warm" | "navy" | "tomica";
                  setTheme(newTheme);
                  localStorage.setItem("app_theme", newTheme);
                }}
                className="text-[11px] font-mono font-bold text-stone-700 bg-transparent border-none focus:outline-none cursor-pointer pr-0.5"
                title="Select interface theme"
              >
                <option value="warm">Warm</option>
                <option value="navy">Navy</option>
                <option value="tomica">Tomica</option>
              </select>
            </div>

            {appIcon && (
              <button
                onClick={handleResetIcon}
                className="p-1 text-stone-400 hover:text-red-600 rounded border border-[#e4ded5] hover:border-red-200 transition-colors cursor-pointer flex items-center justify-center bg-white shadow-xs"
                title="Remove custom icon"
              >
                <Trash size={11} />
              </button>
            )}
          </div>
        </div>

        {/* Dynamic Portfolio Statistics Bar (Compressed to a clean single row) */}
        <div className="hidden lg:flex items-center gap-4 text-[11px] font-mono border border-[#e4ded5] bg-white px-3.5 py-1.5 rounded-xl text-[#1a1a1a]">
          <div className="flex items-center gap-1.5">
            <Database size={11} className="text-[#1a1a1a]" />
            <span>Total models: <strong className="text-[#1a1a1a]">{stats.totalCount}</strong></span>
          </div>
          
          <div className="h-3 w-px bg-[#e4ded5]" />

          <div className="flex items-center gap-1.5">
            <Star size={11} className="text-amber-600 fill-amber-600" />
            <span>Starred: <strong className="text-[#1a1a1a]">{stats.favoriteCount}</strong></span>
          </div>

          <div className="h-3 w-px bg-[#e4ded5]" />

          {/* Roll Random Car Selection takes over the place of Brands/Scales */}
          <div className="flex items-center gap-1">
            <span className="text-[10px] font-mono font-bold text-[#1a1a1a] flex items-center gap-1 pr-1">
              <Sparkles size={11} className="text-amber-600 fill-amber-600" />
              Random roller:
            </span>
            <div className="flex gap-1 pl-1">
              <button 
                onClick={() => handleRandomSelect(1)}
                className="px-1.5 py-0.5 bg-white border border-[#e4ded5] hover:border-stone-400 text-[10px] font-mono font-bold rounded cursor-pointer text-[#1a1a1a] hover:bg-stone-50 transition-colors"
                title="Roll 1 random car"
              >
                1
              </button>
              <button 
                onClick={() => handleRandomSelect(3)}
                className="px-1.5 py-0.5 bg-white border border-[#e4ded5] hover:border-stone-400 text-[10px] font-mono font-bold rounded cursor-pointer text-[#1a1a1a] hover:bg-stone-50 transition-colors"
                title="Roll 3 random cars"
              >
                3
              </button>
              <button 
                onClick={() => handleRandomSelect(5)}
                className="px-1.5 py-0.5 bg-white border border-[#e4ded5] hover:border-stone-400 text-[10px] font-mono font-bold rounded cursor-pointer text-[#1a1a1a] hover:bg-stone-50 transition-colors"
                title="Roll 5 random cars"
              >
                5
              </button>
            </div>
          </div>
        </div>

        {/* Export ZIP Button on the right to easily let user download code */}
        <div className="flex items-center gap-2">
          <button
            onClick={() => {
              setToast({ message: "Assembling your full project files into a ZIP... Please wait.", type: "info" });
              window.location.href = "/api/download-zip";
            }}
            className="px-3 py-1.5 text-xs font-mono font-bold bg-[#1a1a1a] hover:bg-[#2e2e2e] text-[#fdfbf7] rounded-xl flex items-center gap-1.5 shadow-md cursor-pointer transition-all border border-[#2b2b2b]"
            title="Download full project source code as a ZIP archive"
          >
            <Download size={13} className="text-[#faf6ee]" />
            <span>Export ZIP</span>
          </button>
        </div>

      </header>

      {/* Main 3-Column Dashboard Grid Layout */}
      <div className="flex-1 flex overflow-hidden">
        
        {/* Left Column (320px - 360px) - Catalogue File List & Search Filters */}
        <aside className="w-80 md:w-96 flex-shrink-0 h-full">
          <LeftPanel
            cars={cars}
            selectedIds={selectedIds}
            activeId={activeCarId}
            filters={filters}
            setFilters={setFilters}
            onCarSelect={handleCarSelect}
            onSelectAll={handleSelectAll}
            onClearSelection={handleClearSelection}
            onAddCar={handleAddCar}
            onAddCars={handleAddCars}
            onUpdateAllCars={handleUpdateAllCars}
            onDeleteCar={handleDeleteCar}
            onRandomSelect={handleRandomSelect}
            onAddTagToCar={handleAddTagToCar}
            onUnloadAllPhotos={handleUnloadAllPhotos}
          />
        </aside>

        {/* Middle Column (Flex 1) - Inspection Canvas & Collage Studio */}
        <main className="flex-1 h-full min-w-0">
          <MiddlePanel
            selectedCars={selectedCars}
            activeCar={activeCar}
            onToggleFavorite={handleToggleFavorite}
            onRemoveFromSelection={(id) => handleCarSelect(id, true)}
            onClearSelection={handleClearSelection}
            currentNuggets={activeCar ? nuggetCache[activeCar.filename] : null}
            nuggetsLoading={nuggetsLoading}
            nuggetsError={nuggetsError}
            onRetryNuggets={handleRetryFetchNuggets}
            onAddTagToCar={handleAddTagToCar}
            onUpdateTags={handleUpdateTags}
            totalCarsCount={cars.length}
            filteredCars={filteredCars}
            onSetActiveCarId={setActiveCarId}
          />
        </main>

        {/* Right Column (320px - 360px) - AI Nuggets & Tag Manager */}
        <aside className="w-80 md:w-96 flex-shrink-0 h-full">
          <RightPanel
            cars={cars}
            activeCar={activeCar}
            selectedIds={selectedIds}
            onClearSelection={handleClearSelection}
            onSelectAll={handleSelectAll}
            onUpdateAllCars={handleUpdateAllCars}
            onUpdateTags={handleUpdateTags}
            onUpdateNotes={handleUpdateNotes}
            onUpdateCarDetails={handleUpdateCarDetails}
            nuggetCache={nuggetCache}
            setNuggetCache={setNuggetCache}
            nuggetsLoading={nuggetsLoading}
            nuggetsError={nuggetsError}
            onRetryNuggets={handleRetryFetchNuggets}
            onAddTagToCar={handleAddTagToCar}
          />
        </aside>

      </div>

      {/* Custom Non-Blocking Confirmation Modal Overlay */}
      <AnimatePresence>
        {modalConfig && (
          <div className="fixed inset-0 bg-black/60 backdrop-blur-xs flex items-center justify-center p-4 z-[20000]">
            <motion.div
              initial={{ scale: 0.95, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              exit={{ scale: 0.95, opacity: 0 }}
              className="bg-[#faf6ee] border border-[#e4ded5] rounded-2xl max-w-md w-full shadow-2xl p-6 relative overflow-hidden"
            >
              <div className="flex items-center gap-3 mb-3 border-b border-[#e4ded5] pb-3">
                <AlertCircle size={22} className={modalConfig.isDestructive ? "text-red-600" : "text-[#1a1a1a]"} />
                <h3 className="text-base font-bold font-mono tracking-wide text-stone-900">{modalConfig.title}</h3>
              </div>
              <p className="text-stone-650 text-xs font-medium leading-relaxed mb-6 font-sans">
                {modalConfig.message}
              </p>
              <div className="flex items-center justify-end gap-3 font-mono text-[11px] font-bold">
                <button
                  type="button"
                  onClick={() => setModalConfig(null)}
                  className="px-4 py-2 border border-[#e4ded5] bg-white hover:bg-stone-50 text-stone-700 rounded-lg cursor-pointer transition-all"
                >
                  Cancel
                </button>
                <button
                  type="button"
                  onClick={async () => {
                    await modalConfig.onConfirm();
                  }}
                  className={`px-4 py-2 text-white rounded-lg cursor-pointer transition-all ${
                    modalConfig.isDestructive 
                      ? "bg-red-600 hover:bg-red-700 active:bg-red-800" 
                      : "bg-[#1a1a1a] hover:bg-[#2b2b2b]"
                  }`}
                >
                  {modalConfig.confirmLabel}
                </button>
              </div>
            </motion.div>
          </div>
        )}
      </AnimatePresence>

      {/* Custom Non-Blocking Toast Notification */}
      <AnimatePresence>
        {toast && (
          <motion.div
            initial={{ opacity: 0, y: 50, x: "-50%" }}
            animate={{ opacity: 1, y: 0, x: "-50%" }}
            exit={{ opacity: 0, y: 20, x: "-50%" }}
            className="fixed bottom-6 left-1/2 -translate-x-1/2 z-[20010] flex items-center gap-2.5 px-4.5 py-3 rounded-xl border shadow-xl font-medium text-xs font-sans max-w-sm sm:max-w-md"
            style={{
              backgroundColor: toast.type === "error" ? "#fef2f2" : toast.type === "info" ? "#eff6ff" : "#f0fdf4",
              color: toast.type === "error" ? "#991b1b" : toast.type === "info" ? "#1e40af" : "#166534",
              borderColor: toast.type === "error" ? "#fca5a5" : toast.type === "info" ? "#93c5fd" : "#86efac"
            }}
          >
            {toast.type === "error" ? (
              <AlertCircle size={16} />
            ) : toast.type === "info" ? (
              <Info size={16} />
            ) : (
              <CheckCircle size={16} />
            )}
            <span className="flex-1 leading-normal">{toast.message}</span>
            <button 
              type="button"
              onClick={() => setToast(null)}
              className="text-stone-400 hover:text-stone-600 cursor-pointer p-0.5 rounded"
            >
              <X size={14} />
            </button>
          </motion.div>
        )}
      </AnimatePresence>

    </div>
  );
}
