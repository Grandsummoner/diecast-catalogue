export interface DiecastCar {
  id: string;
  filename: string;
  scale: "1:18" | "1:24" | "1:43" | "1:64" | "1:87";
  brand: string;
  imageUrl: string;
  tags: string[];
  category?: string;
  dateAdded: string;
  notes: string;
  isFavorite: boolean;
  condition: "Mint" | "Near Mint" | "Excellent" | "Good" | "Loose";
}

export type ScaleType = "1:18" | "1:24" | "1:43" | "1:64" | "1:87";
export type ConditionType = "Mint" | "Near Mint" | "Excellent" | "Good" | "Loose";

export interface FilterState {
  searchQuery: string;
  scales: ScaleType[];
  brands: string[];
  tags: string[];
  categories: string[];
  conditions: ConditionType[];
  onlyFavorites: boolean;
}

export interface AINuggets {
  detectedYear: string;
  detectedMake: string;
  detectedModel: string;
  history: string;
  specs: { label: string; value: string }[];
  facts: string[];
  collectorGuide: {
    collectabilityScore: number;
  };
}

export type CollageTemplate = "grid" | "strip" | "polaroid" | "masonry" | "stacked";
export type CollageBackground = "slate" | "dark" | "leather" | "carbon" | "wood" | "minimal" | "sunset" | "beach" | "space" | "vibrant";

export interface CollageConfig {
  template: CollageTemplate;
  background: CollageBackground;
  showLabels: boolean;
  showScaleTags: boolean;
  borderColor: string;
  spacing: number;
  title: string;
  cardRadius?: "none" | "md" | "lg" | "2xl";
  photoRatio?: "video" | "square" | "auto";
  shadowSize?: "none" | "sm" | "md" | "xl";
  textAlignment?: "left" | "center" | "right";
  photoFit?: "cover" | "contain";
  diceVersion?: number;
  isWhacky?: boolean;
  activeEffects?: string[];
}
