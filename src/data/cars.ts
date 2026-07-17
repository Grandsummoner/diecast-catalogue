import { DiecastCar } from "../types";

export const INITIAL_CARS: DiecastCar[] = [];

// Helper to parse filename into clean components
export function parseFilename(filename: string) {
  const clean = filename.replace(/\.(jpg|jpeg|png|webp|gif)$/i, "");
  const parts = clean.split("_");
  
  // Try to find a year
  const yearIndex = parts.findIndex(p => /^(19|20)\d{2}$/.test(p));
  const year = yearIndex !== -1 ? parts[yearIndex] : "";
  
  // Brand/Make is usually the word after the year
  let make = "";
  let modelParts: string[] = [];
  
  if (yearIndex !== -1 && yearIndex < parts.length - 1) {
    make = parts[yearIndex + 1];
    // Capitalize make
    make = make.charAt(0).toUpperCase() + make.slice(1);
    
    // Model is everything after the make, minus standard colors
    const colors = ["black", "blue", "red", "orange", "white", "yellow", "purple", "green", "silver", "gold", "matte", "grey", "gray", "carbon"];
    modelParts = parts.slice(yearIndex + 2).filter(p => !colors.includes(p.toLowerCase()));
  } else {
    // Fallback if no year found
    make = parts[0] ? parts[0].charAt(0).toUpperCase() + parts[0].slice(1) : "";
    modelParts = parts.slice(1);
  }
  
  // If make is empty or somehow became "Unknown" or "Unknown Make", clean it up
  if (!make || make.toLowerCase() === "unknown" || make.toLowerCase() === "unknown make") {
    make = "Model";
  }
  
  // Format model nicely
  const model = modelParts.map(p => p.charAt(0).toUpperCase() + p.slice(1)).join(" ") || "Custom";
  
  // Extract colors
  const colorsList = ["black", "blue", "red", "orange", "white", "yellow", "purple", "green", "silver", "gold", "grey", "gray", "matte"];
  const detectedColors = parts.filter(p => colorsList.includes(p.toLowerCase())).map(p => p.charAt(0).toUpperCase() + p.slice(1));
  const color = detectedColors.length > 0 ? detectedColors.join(" & ") : "Standard";

  return {
    year,
    make,
    model: model === make ? model : model,
    color,
    fullName: `${year} ${make} ${model}`.trim().replace(/\s+/g, " ")
  };
}
