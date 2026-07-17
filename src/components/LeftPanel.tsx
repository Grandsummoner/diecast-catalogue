import React, { useState, useMemo, useRef, useEffect } from "react";
import { 
  Search, Grid, List, Sparkles, Plus, Image as ImageIcon, CheckCircle, 
  Trash2, Star, Filter, RotateCcw, HelpCircle, AlertCircle,
  FolderOpen, UploadCloud, RefreshCw, Layers, Sliders, Info
} from "lucide-react";
import { motion, AnimatePresence } from "motion/react";
import { DiecastCar, ScaleType, ConditionType, FilterState } from "../types";
import { parseFilename } from "../data/cars";
import { storePhoto } from "../utils/photoStorage";

interface LeftPanelProps {
  cars: DiecastCar[];
  selectedIds: string[];
  activeId: string | null;
  filters: FilterState;
  setFilters: React.Dispatch<React.SetStateAction<FilterState>>;
  onCarSelect: (id: string, isMulti?: boolean) => void;
  onSelectAll: (ids: string[]) => void;
  onClearSelection: () => void;
  onAddCar: (car: DiecastCar) => void;
  onAddCars: (cars: DiecastCar[]) => void;
  onUpdateAllCars: (cars: DiecastCar[]) => void;
  onDeleteCar: (id: string) => void;
  onRandomSelect: (count: number) => void;
  onAddTagToCar?: (carId: string, tag: string) => void;
  onUnloadAllPhotos: (idsToUnload?: string[]) => void;
}

export default function LeftPanel({
  cars,
  selectedIds,
  activeId,
  filters,
  setFilters,
  onCarSelect,
  onSelectAll,
  onClearSelection,
  onAddCar,
  onAddCars,
  onUpdateAllCars,
  onDeleteCar,
  onRandomSelect,
  onAddTagToCar,
  onUnloadAllPhotos
}: LeftPanelProps) {
  const [viewMode, setViewMode] = useState<"grid" | "list">("grid");
  const [showAddForm, setShowAddForm] = useState(false);
  const [showFiltersDrawer, setShowFiltersDrawer] = useState(false);

  // Drag and drop states
  const [isDragging, setIsDragging] = useState(false);

  // Pagination states
  const [currentPage, setCurrentPage] = useState(1);
  const ITEMS_PER_PAGE = 50;

  // Mass Tagging quick forms
  const [bulkBrand, setBulkBrand] = useState("");
  const [bulkScale, setBulkScale] = useState<ScaleType | "">("");
  const [bulkAddTag, setBulkAddTag] = useState("");

  // Bulk importer states - simplified to instantly load images on selection
  const [relinkSuccessCount, setRelinkSuccessCount] = useState<number | null>(null);
  const [relinkError, setRelinkError] = useState("");

  // Refs for directory / file inputs
  const folderInputRef = useRef<HTMLInputElement>(null);
  const filesInputRef = useRef<HTMLInputElement>(null);
  const relinkInputRef = useRef<HTMLInputElement>(null);

  const processAndAddFiles = async (filesList: File[]) => {
    if (filesList.length === 0) return;

    const POPULAR_BRANDS = [
      "Matchbox", "Tomica", "Majorette", "Greenlight", 
      "Inno64", "Tarmac Works", "MiniGT", "Auto World", "M2 Machines", 
      "Johnny Lightning", "Maisto", "Bburago", "Jada Toys", "Kyosho", 
      "Schuco", "Welly", "Siku", "Norev", "Solido", "Autoart"
    ];
    const scaleRegex = /\b1[:\-/]?(18|24|43|64|87)\b/;

    const carsToAdd: DiecastCar[] = [];
    for (let index = 0; index < filesList.length; index++) {
      const file = filesList[index];
      const filename = file.name;
      const relativePath = file.webkitRelativePath || file.name;
      
      const parts = relativePath.split("/");
      const folders = parts.slice(0, parts.length - 1);

      // Detect scale
      let scale: ScaleType = "1:64";
      let foundScale: string | null = null;
      for (let i = folders.length - 1; i >= 0; i--) {
        const match = folders[i].match(scaleRegex);
        if (match) {
          foundScale = `1:${match[1]}`;
          break;
        }
      }
      if (!foundScale) {
        const match = filename.match(scaleRegex);
        if (match) {
          foundScale = `1:${match[1]}`;
        }
      }
      if (foundScale) {
        scale = foundScale as ScaleType;
      }

      // Detect brand
      let brand = "Kyosho";
      let foundBrand: string | null = null;
      const findBrand = (text: string) => {
        const lowerText = text.toLowerCase();
        return POPULAR_BRANDS.find(b => lowerText.includes(b.toLowerCase()));
      };

      for (let i = folders.length - 1; i >= 0; i--) {
        const b = findBrand(folders[i]);
        if (b) {
          foundBrand = b;
          break;
        }
      }
      if (!foundBrand) {
        const b = findBrand(filename);
        if (b) {
          foundBrand = b;
        }
      }
      if (foundBrand) {
        brand = foundBrand;
      }

      // Extract tags: Photos start with exactly 1 tag (folder name or "Imported")
      const tags: string[] = [];
      if (folders.length > 0) {
        const lastFolder = folders[folders.length - 1];
        const cleanFolder = lastFolder.replace(/[_\-]/g, " ").trim();
        const capitalized = cleanFolder.split(" ").map(w => w.charAt(0).toUpperCase() + w.slice(1)).join(" ");
        if (capitalized) {
          tags.push(capitalized.slice(0, 12));
        }
      }
      if (tags.length === 0) {
        tags.push("Imported");
      }

      const id = `bulk-${Date.now()}-${index}`;

      // Save file content in IndexedDB
      await storePhoto(id, file);

      carsToAdd.push({
        id,
        filename,
        scale,
        brand,
        imageUrl: URL.createObjectURL(file), // Generate active blob URL
        tags,
        dateAdded: new Date().toISOString().split("T")[0],
        notes: `Folder path: ${relativePath}`,
        isFavorite: false,
        condition: "Mint"
      } as DiecastCar);
    }

    if (carsToAdd.length > 0) {
      // Replaces the placeholder fallback menu entirely with your custom photos
      onUpdateAllCars(carsToAdd);
      setShowAddForm(false);
      
      // Auto-select all newly added files so they can be mass-tagged instantly
      const newIds = carsToAdd.map(c => c.id);
      if (newIds.length > 0) {
        onSelectAll(newIds);
        onCarSelect(newIds[0]); // focus on the first item
      }
    }
  };

  const handleFolderChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = Array.from(e.target.files || []) as File[];
    const imageFiles = files.filter(f => f.type.startsWith("image/") || /\.(jpg|jpeg|png|webp|gif|bmp)$/i.test(f.name));
    processAndAddFiles(imageFiles);
  };

  const handleFilesChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = Array.from(e.target.files || []) as File[];
    const imageFiles = files.filter(f => f.type.startsWith("image/") || /\.(jpg|jpeg|png|webp|gif|bmp)$/i.test(f.name));
    processAndAddFiles(imageFiles);
  };

  const handleRelinkFolder = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = Array.from(e.target.files || []) as File[];
    if (files.length === 0) return;

    const imageFiles = files.filter(f => f.type.startsWith("image/") || /\.(jpg|jpeg|png|webp|gif|bmp)$/i.test(f.name));
    
    let successCount = 0;
    const updatedCars = await Promise.all(cars.map(async (car) => {
      const cleanCarFilename = car.filename.toLowerCase().trim();
      const matchedFile = imageFiles.find(f => {
        const cleanFileName = f.name.toLowerCase().trim();
        const cleanRelPath = f.webkitRelativePath ? f.webkitRelativePath.toLowerCase().trim() : "";
        return cleanFileName === cleanCarFilename || 
               cleanRelPath.endsWith("/" + cleanCarFilename) || 
               cleanRelPath.endsWith("\\" + cleanCarFilename) ||
               cleanRelPath === cleanCarFilename;
      }) as File | undefined;

      if (matchedFile) {
        successCount++;
        await storePhoto(car.id, matchedFile);
        return {
          ...car,
          imageUrl: URL.createObjectURL(matchedFile)
        };
      }
      return car;
    }));

    if (successCount > 0) {
      onUpdateAllCars(updatedCars);
      setRelinkSuccessCount(successCount);
      setRelinkError("");
      setTimeout(() => setRelinkSuccessCount(null), 6000);
    } else {
      setRelinkError("No matches found. Ensure the directory contains matching image filenames.");
      setRelinkSuccessCount(null);
    }
  };

  const hasLocalBlobRefs = useMemo(() => {
    return cars.some(car => car.imageUrl.startsWith("blob:"));
  }, [cars]);

  // Get all unique brands, tags, and categories for filtering list
  const allBrands = useMemo(() => {
    const brandsSet = new Set(cars.map(c => c.brand));
    return Array.from(brandsSet).sort();
  }, [cars]);

  const allTags = useMemo(() => {
    const tagsSet = new Set<string>();
    cars.forEach(c => c.tags.forEach(t => {
      if (t.trim().toLowerCase() !== "imported to delete") {
        tagsSet.add(t);
      }
    }));
    return Array.from(tagsSet).sort();
  }, [cars]);

  const allCategories = useMemo(() => {
    const catsSet = new Set<string>();
    cars.forEach(c => {
      if (c.category) {
        catsSet.add(c.category);
      }
    });
    return Array.from(catsSet).sort();
  }, [cars]);



  // Toggle dynamic filter tags
  const toggleFilterScale = (scale: ScaleType) => {
    setFilters(prev => ({
      ...prev,
      scales: prev.scales.includes(scale) 
        ? prev.scales.filter(s => s !== scale)
        : [...prev.scales, scale]
    }));
  };

  const toggleFilterBrand = (brand: string) => {
    setFilters(prev => ({
      ...prev,
      brands: prev.brands.includes(brand)
        ? prev.brands.filter(b => b !== brand)
        : [...prev.brands, brand]
    }));
  };

  const toggleFilterTag = (tag: string) => {
    setFilters(prev => ({
      ...prev,
      tags: prev.tags.includes(tag)
        ? prev.tags.filter(t => t !== tag)
        : [...prev.tags, tag]
    }));
  };

  const resetAllFilters = () => {
    setFilters({
      searchQuery: "",
      scales: [],
      brands: [],
      tags: [],
      categories: [],
      conditions: [],
      onlyFavorites: false
    });
  };

  // Reset pagination on search, filters change
  useEffect(() => {
    setCurrentPage(1);
  }, [filters]);

  // Filter the diecasts
  const filteredCars = useMemo(() => {
    return cars.filter(car => {
      // Search text match
      const query = filters.searchQuery.toLowerCase();
      const parsed = parseFilename(car.filename);
      const matchesSearch = car.filename.toLowerCase().includes(query) ||
        parsed.fullName.toLowerCase().includes(query) ||
        car.brand.toLowerCase().includes(query) ||
        car.tags.some(t => t.toLowerCase().includes(query)) ||
        (car.category && car.category.toLowerCase().includes(query)) ||
        car.notes.toLowerCase().includes(query);

      // Scale filter
      const matchesScale = filters.scales.length === 0 || filters.scales.includes(car.scale);
      // Brand filter
      const matchesBrand = filters.brands.length === 0 || filters.brands.includes(car.brand);
      // Tags filter
      const matchesTags = filters.tags.length === 0 || car.tags.some(t => filters.tags.includes(t));
      // Categories filter
      const matchesCategories = !filters.categories || filters.categories.length === 0 || (car.category && filters.categories.includes(car.category));
      // Favorites filter
      const matchesFavorite = !filters.onlyFavorites || car.isFavorite;

      return matchesSearch && matchesScale && matchesBrand && matchesTags && matchesCategories && matchesFavorite;
    });
  }, [cars, filters]);

  // Calculate Paginated subset
  const paginatedCars = useMemo(() => {
    const start = (currentPage - 1) * ITEMS_PER_PAGE;
    return filteredCars.slice(start, start + ITEMS_PER_PAGE);
  }, [filteredCars, currentPage]);

  const totalPages = Math.ceil(filteredCars.length / ITEMS_PER_PAGE);

  return (
    <div className="flex flex-col h-full bg-[#fdfbf7] border-r border-[#e4ded5] text-[#1a1a1a]">
      
      {/* Search Header */}
      <div className="p-3 border-b border-[#e4ded5]">
        <div className="flex items-center gap-1.5 w-full">
          {/* Search Input */}
          <div className="relative flex-1">
            <span className="absolute inset-y-0 left-0 flex items-center pl-2.5 pointer-events-none text-stone-400">
              <Search size={14} />
            </span>
            <input
              type="text"
              value={filters.searchQuery}
              onChange={(e) => setFilters(p => ({ ...p, searchQuery: e.target.value }))}
              placeholder="Search by filename..."
              className="w-full pl-8 pr-2 py-1.5 bg-white border border-[#e4ded5] rounded-lg text-xs text-[#1a1a1a] placeholder-stone-450 focus:outline-none focus:border-[#1a1a1a] focus:ring-1 focus:ring-[#1a1a1a]"
            />
          </div>

          {/* Grid/List View Toggle */}
          <div className="flex items-center bg-white p-0.5 rounded-lg border border-[#e4ded5] flex-shrink-0">
            <button 
              onClick={() => setViewMode("grid")}
              className={`p-1 rounded transition-colors cursor-pointer ${viewMode === "grid" ? "bg-[#1a1a1a] text-[#fdfbf7]" : "text-stone-450 hover:text-stone-850"}`}
              title="Thumbnail grid"
            >
              <Grid size={13} />
            </button>
            <button 
              onClick={() => setViewMode("list")}
              className={`p-1 rounded transition-colors cursor-pointer ${viewMode === "list" ? "bg-[#1a1a1a] text-[#fdfbf7]" : "text-stone-450 hover:text-stone-850"}`}
              title="Filenames list"
            >
              <List size={13} />
            </button>
          </div>

          {/* Filters Toggle button */}
          <button
            onClick={() => setShowFiltersDrawer(!showFiltersDrawer)}
            className={`p-1.5 rounded-lg border transition-colors cursor-pointer flex-shrink-0 ${showFiltersDrawer || filters.scales.length > 0 || filters.brands.length > 0 || filters.tags.length > 0 || filters.onlyFavorites ? "border-[#1a1a1a] bg-[#1a1a1a] text-[#fdfbf7]" : "border-[#e4ded5] bg-white text-stone-600 hover:text-[#1a1a1a]"}`}
            title="Advanced filters"
          >
            <Filter size={13} />
          </button>

          {/* Starred Toggle */}
          <button
            onClick={() => setFilters(prev => ({ ...prev, onlyFavorites: !prev.onlyFavorites }))}
            className={`p-1.5 rounded-lg border transition-colors cursor-pointer flex-shrink-0 ${filters.onlyFavorites ? "border-amber-600 bg-amber-50 text-amber-800" : "border-[#e4ded5] bg-white text-stone-600 hover:text-[#1a1a1a]"}`}
            title="Starred files"
          >
            <Star size={13} fill={filters.onlyFavorites ? "currentColor" : "none"} />
          </button>

          {/* Add File button */}
          <button
            onClick={() => setShowAddForm(!showAddForm)}
            className={`p-1.5 rounded-lg transition-colors cursor-pointer flex-shrink-0 ${showAddForm ? "bg-amber-600 text-white" : "bg-[#1a1a1a] hover:bg-[#2e2e2e] text-[#fdfbf7]"}`}
            title="Add file"
          >
            <Plus size={13} />
          </button>
        </div>
      </div>

      {/* Advanced Filter Drawer */}
      <AnimatePresence>
        {showFiltersDrawer && (
          <motion.div
            initial={{ height: 0, opacity: 0 }}
            animate={{ height: "auto", opacity: 1 }}
            exit={{ height: 0, opacity: 0 }}
            className="overflow-hidden border-b border-[#e4ded5] bg-[#faf8f4]/80"
          >
            <div className="p-4 space-y-4 text-xs">
              <div className="flex items-center justify-between">
                <span className="text-[10px] font-mono font-bold text-stone-600">Advanced filters</span>
                <button 
                  onClick={resetAllFilters}
                  className="flex items-center gap-1 text-stone-500 hover:text-[#1a1a1a] transition-colors cursor-pointer"
                >
                  <RotateCcw size={12} />
                  <span>Reset</span>
                </button>
              </div>

              {/* Scales selection */}
              <div>
                <span className="block mb-2 text-[10px] text-stone-500 font-semibold font-mono">Scale</span>
                <div className="flex flex-wrap gap-1.5">
                  {(["1:18", "1:24", "1:43", "1:64", "1:87"] as ScaleType[]).map(sc => (
                    <button
                      key={sc}
                      onClick={() => toggleFilterScale(sc)}
                      className={`px-2 py-1 rounded border transition-colors cursor-pointer ${filters.scales.includes(sc) ? "bg-[#1a1a1a] text-[#fdfbf7] border-[#1a1a1a]" : "bg-white text-stone-600 border-[#e4ded5] hover:border-stone-400"}`}
                    >
                      {sc}
                    </button>
                  ))}
                </div>
              </div>

              {/* Brands selection */}
              <div>
                <span className="block mb-2 text-[10px] text-stone-500 font-semibold font-mono">Brands</span>
                <div className="flex flex-wrap gap-1.5 max-h-24 overflow-y-auto pr-1">
                  {allBrands.map(br => (
                    <button
                      key={br}
                      onClick={() => toggleFilterBrand(br)}
                      className={`px-2 py-1 rounded border transition-colors cursor-pointer ${filters.brands.includes(br) ? "bg-[#1a1a1a] text-[#fdfbf7] border-[#1a1a1a]" : "bg-white text-stone-600 border-[#e4ded5] hover:border-stone-400"}`}
                    >
                      {br}
                    </button>
                  ))}
                </div>
              </div>

              {/* Categories selection */}
              <div>
                <span className="block mb-2 text-[10px] text-stone-500 font-semibold font-mono">Categories</span>
                <div className="flex flex-wrap gap-1.5 max-h-24 overflow-y-auto pr-1">
                  {allCategories.map(cat => (
                    <button
                      key={cat}
                      onClick={() => {
                        setFilters(prev => ({
                          ...prev,
                          categories: prev.categories.includes(cat)
                            ? prev.categories.filter(c => c !== cat)
                            : [...prev.categories, cat]
                        }));
                      }}
                      className={`px-2 py-1 rounded border transition-colors cursor-pointer ${filters.categories?.includes(cat) ? "bg-[#1a1a1a] text-[#fdfbf7] border-[#1a1a1a]" : "bg-white text-stone-600 border-[#e4ded5] hover:border-stone-400"}`}
                    >
                      {cat}
                    </button>
                  ))}
                  {allCategories.length === 0 && (
                    <span className="text-stone-400 text-[10px] italic font-mono">No categories created yet.</span>
                  )}
                </div>
              </div>

              {/* Tags selection */}
              <div>
                <span className="block mb-2 text-[10px] text-stone-500 font-semibold font-mono">Tags</span>
                <div className="flex flex-wrap gap-1.5 max-h-24 overflow-y-auto pr-1">
                  {allTags.map(tg => (
                    <button
                      key={tg}
                      onClick={() => toggleFilterTag(tg)}
                      className={`px-2 py-1 rounded border transition-colors cursor-pointer ${filters.tags.includes(tg) ? "bg-[#1a1a1a] text-[#fdfbf7] border-[#1a1a1a]" : "bg-white text-stone-600 border-[#e4ded5] hover:border-stone-400"}`}
                    >
                      {tg}
                    </button>
                  ))}
                </div>
              </div>
            </div>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Multipurpose Importer Panel */}
      <AnimatePresence>
        {showAddForm && (
          <motion.div
            initial={{ height: 0, opacity: 0 }}
            animate={{ height: "auto", opacity: 1 }}
            exit={{ height: 0, opacity: 0 }}
            className="overflow-hidden bg-[#faf8f4] border-b border-[#e4ded5] p-4 space-y-3"
          >
            <div className="flex items-center justify-between">
              <span className="font-bold font-mono text-stone-800 text-xs flex items-center gap-1.5 tracking-wide">
                <UploadCloud size={14} className="text-stone-700" />
                Multipurpose importer
              </span>
              <button
                type="button"
                onClick={() => setShowAddForm(false)}
                className="text-stone-500 hover:text-black font-mono font-bold text-[10px] cursor-pointer"
              >
                Cancel
              </button>
            </div>

            {/* Draggable Drop Zone */}
            <div
              onDragOver={(e) => { e.preventDefault(); setIsDragging(true); }}
              onDragLeave={() => setIsDragging(false)}
              onDrop={(e) => {
                e.preventDefault();
                setIsDragging(false);
                const files = Array.from(e.dataTransfer.files) as File[];
                const imageFiles = files.filter(f => f.type.startsWith("image/") || /\.(jpg|jpeg|png|webp|gif|bmp)$/i.test(f.name));
                processAndAddFiles(imageFiles);
              }}
              onClick={() => filesInputRef.current?.click()}
              className={`border-2 border-dashed rounded-xl p-6 flex flex-col items-center justify-center gap-2 text-center cursor-pointer transition-all duration-200 ${isDragging ? "border-amber-600 bg-amber-50/40" : "border-[#e4ded5] bg-white hover:border-stone-400 hover:bg-stone-50/50"}`}
            >
              <UploadCloud size={28} className={`text-stone-400 ${isDragging ? "text-amber-600 animate-bounce" : "transition-transform hover:scale-110"}`} />
              <div>
                <span className="font-bold block text-stone-850 text-xs">Drag & drop photos or folder</span>
                <span className="text-[10px] text-stone-500 font-mono mt-0.5 block">Or click to select multiple files</span>
              </div>
            </div>

            {/* Folder selection helper button */}
            <div className="flex flex-col gap-2">
              <div className="flex gap-2">
                <button
                  type="button"
                  onClick={() => filesInputRef.current?.click()}
                  className="flex-1 py-2 px-3 border border-[#e4ded5] bg-white hover:bg-stone-50 font-mono font-bold text-[10px] rounded-lg text-stone-700 cursor-pointer flex items-center justify-center gap-1.5 transition-all shadow-sm"
                >
                  <ImageIcon size={13} />
                  <span>Select files</span>
                </button>
                <button
                  type="button"
                  onClick={() => folderInputRef.current?.click()}
                  className="flex-1 py-2 px-3 border border-[#e4ded5] bg-white hover:bg-stone-50 font-mono font-bold text-[10px] rounded-lg text-stone-700 cursor-pointer flex items-center justify-center gap-1.5 transition-all shadow-sm"
                >
                  <FolderOpen size={13} />
                  <span>Select folder</span>
                </button>
              </div>
            </div>

            {/* Hidden inputs */}
            <input
              type="file"
              webkitdirectory=""
              directory=""
              multiple
              ref={folderInputRef}
              onChange={handleFolderChange}
              className="hidden"
            />
            <input
              type="file"
              multiple
              accept="image/*"
              ref={filesInputRef}
              onChange={handleFilesChange}
              className="hidden"
            />

            <div className="bg-stone-100 p-2.5 rounded-lg text-[10px] font-mono text-stone-500 leading-relaxed">
              💡 Brands, scales, and custom category tags are parsed instantly from the imported file names and directories.
            </div>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Multi-Select Stats Bar */}
      {filteredCars.length > 0 && (
        <div className="bg-stone-100 p-2 border-b border-[#e4ded5] text-xs flex flex-wrap items-center justify-between gap-2">
          <span className="text-stone-800 font-semibold font-mono text-[10px] pl-1">
            Selected: {selectedIds.length} car{selectedIds.length !== 1 ? "s" : ""}
          </span>
          <div className="flex gap-1.5 items-center">
            <button
              onClick={() => onSelectAll(filteredCars.map(c => c.id))}
              className="bg-green-600 hover:bg-green-700 text-white font-semibold font-mono text-[10px] px-2.5 py-1 rounded-md transition-colors cursor-pointer shadow-xs"
            >
              Select all
            </button>
            {selectedIds.length > 0 ? (
              <button
                onClick={onClearSelection}
                className="bg-blue-600 hover:bg-blue-700 text-white font-semibold font-mono text-[10px] px-2.5 py-1 rounded-md transition-colors cursor-pointer shadow-xs"
              >
                Deselect all
              </button>
            ) : (
              <button
                disabled
                className="bg-stone-200 text-stone-400 font-semibold font-mono text-[10px] px-2.5 py-1 rounded-md cursor-not-allowed select-none border border-stone-300/30"
              >
                Deselect all
              </button>
            )}
            <button
              type="button"
              onClick={() => onUnloadAllPhotos(selectedIds)}
              className="bg-red-50 hover:bg-red-100 text-red-700 border border-red-200 font-semibold font-mono text-[10px] px-2.5 py-1 rounded-md transition-colors cursor-pointer shadow-xs flex items-center gap-1"
            >
              <Trash2 size={11} />
              <span>{selectedIds.length > 0 ? `Unload (${selectedIds.length})` : "Unload"}</span>
            </button>
          </div>
        </div>
      )}

      {/* Main Catalog List / Scroll area */}
      <div className="flex-1 overflow-y-auto custom-scrollbar p-3 space-y-2">

        <AnimatePresence initial={false}>
          {filteredCars.length === 0 ? (
            <div className="h-64 flex flex-col items-center justify-center text-center p-6 text-stone-500 border border-dashed border-[#e4ded5] rounded-lg">
              <AlertCircle size={32} className="mb-2 text-stone-400" />
              <p className="text-sm font-semibold">No diecast records matched your filters.</p>
              <button 
                onClick={resetAllFilters}
                className="mt-3 text-xs text-[#1a1a1a] underline hover:text-stone-700 font-semibold"
              >
                Clear Search & Filters
              </button>
            </div>
          ) : viewMode === "list" ? (
            // Filenames List Mode (Visualizing only photos and filenames)
            <div className="space-y-1.5">
              {paginatedCars.map(car => {
                const isSelected = selectedIds.includes(car.id);
                const isActive = activeId === car.id;

                return (
                  <motion.div
                    key={car.id}
                    layoutId={`car-card-${car.id}`}
                    className={`group flex items-center gap-3 p-2 rounded-lg cursor-pointer transition-all border ${isActive ? "bg-white border-[#1a1a1a] shadow-sm text-stone-900" : isSelected ? "bg-[#faf6ee] border-[#d6cfc4]" : "bg-white/80 border-[#e4ded5] hover:border-stone-400 hover:bg-white"}`}
                    onClick={() => onCarSelect(car.id, true)}
                    onDragOver={(e) => {
                      e.preventDefault();
                      e.currentTarget.classList.add("bg-amber-50/50", "border-dashed", "border-amber-400");
                    }}
                    onDragLeave={(e) => {
                      e.currentTarget.classList.remove("bg-amber-50/50", "border-dashed", "border-amber-400");
                    }}
                    onDrop={(e) => {
                      e.preventDefault();
                      e.currentTarget.classList.remove("bg-amber-50/50", "border-dashed", "border-amber-400");
                      const tag = e.dataTransfer.getData("text/plain");
                      if (tag && onAddTagToCar) {
                        onAddTagToCar(car.id, tag);
                      }
                    }}
                  >
                    {/* Multi-Select Checkbox */}
                    <div 
                      onClick={(e) => {
                        e.stopPropagation();
                        onCarSelect(car.id, true);
                      }}
                      className="p-1 cursor-pointer"
                    >
                      <div className={`h-4 w-4 rounded flex items-center justify-center border transition-all ${isSelected ? "bg-[#1a1a1a] border-[#1a1a1a] text-[#fdfbf7]" : "border-[#d6cfc4] hover:border-stone-500 bg-white"}`}>
                        {isSelected && <CheckCircle size={12} className="stroke-[3px]" />}
                      </div>
                    </div>

                    {/* Small Image Preview */}
                    <div className="relative h-10 w-14 flex-shrink-0 bg-stone-100 rounded overflow-hidden border border-[#e4ded5]">
                      <img 
                        src={car.imageUrl} 
                        alt="Car photo thumbnail"
                        referrerPolicy="no-referrer"
                        className="h-full w-full object-cover group-hover:scale-105 transition-transform duration-200"
                      />
                    </div>

                    {/* Filename Display and Tags row below it */}
                    <div className="flex-1 min-w-0 flex flex-col justify-center py-0.5">
                      <div className="font-mono text-xs text-stone-850 font-bold truncate group-hover:text-black transition-colors" title={car.filename}>
                        {car.filename}
                      </div>
                      
                      {/* Separate row of tags in smaller font below filename */}
                      <div className="flex flex-wrap items-center gap-1 mt-1 max-w-full">
                        {car.tags && car.tags.length > 0 && car.tags.map(t => (
                          <span key={t} className="px-1 py-0.2 bg-white border border-stone-200 text-stone-600 rounded-md font-bold text-[8px] font-mono whitespace-nowrap flex items-center gap-0.5" title={`Tag: ${t}`}>
                            <span>🏷️</span>
                            <span className="truncate max-w-[60px]">{t}</span>
                          </span>
                        ))}
                      </div>
                    </div>

                    {/* Compact actions: Star indicator and quick delete */}
                    <div className="flex items-center gap-1.5 opacity-0 group-hover:opacity-100 transition-opacity">
                      {car.isFavorite && (
                        <Star size={11} className="text-amber-500 fill-amber-500" />
                      )}
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          onDeleteCar(car.id);
                        }}
                        className="p-1 text-stone-400 hover:text-red-600 rounded transition-all"
                        title="Delete this diecast record"
                      >
                        <Trash2 size={11} />
                      </button>
                    </div>
                  </motion.div>
                );
              })}
            </div>
          ) : (
            // Thumbnail Grid Mode (Photos and filenames only)
            <div className="grid grid-cols-4 gap-1.5">
              {paginatedCars.map(car => {
                const isSelected = selectedIds.includes(car.id);
                const isActive = activeId === car.id;

                return (
                  <motion.div
                    key={car.id}
                    layoutId={`car-card-${car.id}`}
                    onClick={() => onCarSelect(car.id, true)}
                    title={car.filename}
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
                        onAddTagToCar(car.id, tag);
                      }
                    }}
                    className={`group relative flex flex-col bg-white border rounded overflow-hidden cursor-pointer transition-all ${isActive ? "ring-2 ring-[#1a1a1a] border-[#1a1a1a]" : isSelected ? "border-[#d6cfc4] ring-1 ring-[#d6cfc4] bg-[#faf6ee]" : "border-[#e4ded5] hover:border-stone-400 hover:bg-stone-50"}`}
                  >
                    {/* Selection overlay box */}
                    <div 
                      onClick={(e) => {
                        e.stopPropagation();
                        onCarSelect(car.id, true);
                      }}
                      className="absolute top-1 left-1 z-10 h-4 w-4 bg-white/90 rounded border border-[#e4ded5] flex items-center justify-center text-stone-800 hover:border-stone-400 transition-all"
                    >
                      {isSelected ? (
                        <div className="h-2.5 w-2.5 rounded bg-[#1a1a1a] flex items-center justify-center text-[#fdfbf7]">
                          <CheckCircle size={8} className="stroke-[3px]" />
                        </div>
                      ) : (
                        <div className="h-2 w-2 rounded bg-transparent" />
                      )}
                    </div>

                    {/* Favorite Badge */}
                    {car.isFavorite && (
                      <div className="absolute top-1 right-1 z-10 p-0.5 bg-amber-500 text-white rounded-full shadow-sm">
                        <Star size={8} fill="currentColor" />
                      </div>
                    )}

                    {/* Image */}
                    <div className="relative aspect-video bg-stone-100 overflow-hidden">
                      <img 
                        src={car.imageUrl} 
                        alt="Diecast model"
                        referrerPolicy="no-referrer"
                        className="h-full w-full object-cover group-hover:scale-105 transition-transform duration-200"
                      />
                    </div>

                    {/* Details footer (Filename only!) */}
                    <div className="p-1 bg-[#faf8f4] text-center border-t border-[#e4ded5]">
                      <p className="font-mono text-[9px] text-stone-850 font-bold truncate px-0.5">
                        {car.filename.slice(0, 6)}
                      </p>
                    </div>
                  </motion.div>
                );
              })}
            </div>
          )}
        </AnimatePresence>
      </div>

      {/* Pagination Controls Footer */}
      {totalPages > 1 && (
        <div className="p-3 bg-white border-t border-[#e4ded5] flex items-center justify-between text-xs font-mono">
          <button
            onClick={() => setCurrentPage(p => Math.max(1, p - 1))}
            disabled={currentPage === 1}
            className="px-3 py-1 bg-stone-100 hover:bg-stone-200 disabled:opacity-40 rounded border border-[#e4ded5] font-bold text-stone-700 cursor-pointer disabled:cursor-not-allowed transition-all"
          >
            ← Prev
          </button>
          <span className="text-stone-500 text-[10px] font-bold font-mono">
            Page {currentPage} of {totalPages} ({filteredCars.length} of {cars.length} items)
          </span>
          <button
            onClick={() => setCurrentPage(p => Math.min(totalPages, p + 1))}
            disabled={currentPage === totalPages}
            className="px-3 py-1 bg-stone-100 hover:bg-stone-200 disabled:opacity-40 rounded border border-[#e4ded5] font-bold text-stone-700 cursor-pointer disabled:cursor-not-allowed transition-all"
          >
            Next →
          </button>
        </div>
      )}

      {/* Photo Management Footer */}
      {cars.length > 0 && (
        <div className="p-3 bg-[#faf6ee] border-t border-[#e4ded5] flex flex-col gap-2 flex-shrink-0">
          <div className="flex items-center gap-1 w-full">
            <button
              type="button"
              onClick={() => relinkInputRef.current?.click()}
              className="flex-1 flex items-center justify-center gap-1 bg-stone-100 hover:bg-stone-200 text-stone-700 border border-[#e4ded5] font-mono font-bold px-2 py-1.5 rounded-lg text-[10px] cursor-pointer transition-all"
            >
              <FolderOpen size={10} />
              <span>Fix/Re-link folder</span>
            </button>
            
            {/* Help popover */}
            <div className="relative group/help">
              <button
                type="button"
                className="p-1.5 bg-stone-100 hover:bg-stone-200 border border-[#e4ded5] rounded-lg text-stone-500 hover:text-stone-900 transition-all cursor-help flex items-center justify-center"
                title="Why would I re-link?"
              >
                <HelpCircle size={12} />
              </button>
              <div className="absolute bottom-full right-0 mb-2 w-64 p-3 bg-stone-900 text-[#faf6ee] text-[10px] font-sans font-normal leading-relaxed rounded-lg shadow-xl opacity-0 group-hover/help:opacity-100 transition-opacity pointer-events-none z-50 border border-stone-850">
                <span className="font-mono font-bold text-amber-400 block mb-1 text-[9px] uppercase tracking-wider">IndexedDB Cache:</span>
                The app automatically caches loaded photos locally in your browser so they don't break on reload. If you clear browser cache, click here to re-link your local folder and restore all images.
                <span className="absolute right-3.5 top-full border-8 border-transparent border-t-stone-900" />
              </div>
            </div>
          </div>

          <input
            type="file"
            webkitdirectory=""
            directory=""
            multiple
            ref={relinkInputRef}
            onChange={handleRelinkFolder}
            className="hidden"
          />
          {relinkSuccessCount !== null && (
            <p className="text-[10px] text-emerald-700 font-mono font-bold text-center">
              ✓ Re-linked {relinkSuccessCount} photo{relinkSuccessCount !== 1 ? 's' : ''}!
            </p>
          )}
          {relinkError && (
            <p className="text-[10px] text-red-700 font-mono text-center">
              ✗ {relinkError}
            </p>
          )}
        </div>
      )}

    </div>
  );
}
