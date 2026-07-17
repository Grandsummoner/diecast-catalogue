import { AINuggets } from "../types";
import { parseFilename } from "../data/cars";

export function calculateCollectabilityScore(filename: string): number {
  const parsed = parseFilename(filename);
  const make = (parsed.make || "").trim().toLowerCase();
  const yearStr = (parsed.year || "").trim();
  const year = parseInt(yearStr, 10);
  const full = parsed.fullName.toLowerCase();
  const clean = filename.toLowerCase();

  // 1. BRAND BASE PRESTIGE (Max 5.0 points)
  let baseScore = 6.0; // Standard brand default

  const exoticBrands = ["ferrari", "porsche", "lamborghini", "aston", "mclaren", "bugatti", "koenigsegg", "pagani", "maserati"];
  const premiumBrands = ["shelby", "mercedes", "bmw", "audi", "jaguar", "lexus", "land rover", "cadillac", "lincoln"];
  const performanceBrands = ["ford", "chevrolet", "chevy", "nissan", "toyota", "dodge", "plymouth", "pontiac", "subaru", "mitsubishi", "mazda", "honda"];
  
  if (exoticBrands.some(b => make.includes(b) || clean.includes(b))) {
    baseScore = 8.6;
  } else if (premiumBrands.some(b => make.includes(b) || clean.includes(b))) {
    baseScore = 7.8;
  } else if (performanceBrands.some(b => make.includes(b) || clean.includes(b))) {
    baseScore = 7.0;
  } else if (clean.includes("hot") || clean.includes("matchbox") || clean.includes("model")) {
    baseScore = 5.8;
  }

  // 2. ERA SIGNIFICANCE MODIFIER (Max +1.5 points, Min -0.5 points)
  let eraModifier = 0.0;
  if (!isNaN(year)) {
    if (year >= 1950 && year <= 1972) {
      // Golden era of muscle cars and classics
      eraModifier = 1.2;
    } else if (year < 1950) {
      // Pre-war and vintage classics
      eraModifier = 1.4;
    } else if (year >= 1985 && year <= 1999) {
      // JDM golden age and peak analog supercars
      eraModifier = 0.8;
    } else if (year >= 2000 && year <= 2012) {
      // Early 2000s performance
      eraModifier = 0.3;
    } else if (year >= 2020) {
      // Ultra modern
      eraModifier = -0.2; // New cars are common, lower instant classic score
    }
  }

  // 3. PERFORMANCE & DESIRABILITY KEYWORDS (Max +1.5 points)
  let performanceModifier = 0.0;
  const highPerformanceTerms = ["gt-r", "gtr", "gt3", "gt2", "shelby", "cobra", "turbo", "twin turbo", "supercharged", "v12", "v8", "amg", "nismo", "type r", "m3", "m5", "srt", "hellcat", "demon", "cosworth", "quattro", "2jz", "rb26", "boss", "mach 1"];
  const midPerformanceTerms = ["sport", "coupe", "z28", "ss", "spyder", "roadster", "targa", "rs", "gts", "s line", "st", "rt", "trd"];

  if (highPerformanceTerms.some(term => full.includes(term) || clean.includes(term))) {
    performanceModifier = 1.3;
  } else if (midPerformanceTerms.some(term => full.includes(term) || clean.includes(term))) {
    performanceModifier = 0.6;
  }

  // 4. DETAILED CHARACTERS SEED DIVERSITY (Max +0.6 points)
  // Ensures two slightly different models/colors have uniquely graded scores rather than duplicate rounded numbers
  let stringSum = 0;
  for (let i = 0; i < filename.length; i++) {
    stringSum += filename.charCodeAt(i);
  }
  const varianceModifier = (stringSum % 7) / 10; // Gives 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6

  // Calculate final score
  let finalScore = baseScore + eraModifier + performanceModifier + varianceModifier;

  // Clamping to a realistic range of 3.0 to 10.0
  finalScore = Math.max(3.0, Math.min(10.0, finalScore));

  // Round to 1 decimal place
  return Math.round(finalScore * 10) / 10;
}

export function generateClientFallbackNuggets(filename: string): AINuggets {
  const parsed = parseFilename(filename);
  const nameLower = filename.toLowerCase();
  
  let year = parsed.year || "1990s";
  let make = parsed.make || "Classic";
  let model = parsed.model || "Sports Car";
  
  let history = "";
  let specs: { label: string; value: string }[] = [];
  let facts: string[] = [];
  const score = calculateCollectabilityScore(filename);

  if (nameLower.includes("mustang")) {
    history = `The Ford Mustang, introduced in mid-1964, created the "pony car" class of American muscle cars—affordable, highly styled coupes with long hoods and short rear decks. It became an instant cultural icon, representing freedom, youth, and performance.

Over the decades, special performance variants like the Boss 429, Shelby GT350, and Mach 1 solidified the Mustang's legendary status both on drag strips and in motorsports history. It remains one of the longest-running and most successful continuous nameplates in automotive history, capturing the hearts of collectors worldwide.`;
    specs = [
      { label: "Engine Type", value: "4.7L Windsor V8 / 7.0L Cobra Jet V8" },
      { label: "Horsepower", value: "271 hp - 375 hp" },
      { label: "0-60 mph", value: "5.4 - 7.2 seconds" },
      { label: "Transmission", value: "4-Speed Manual / 3-Speed Automatic" },
      { label: "Top Speed", value: "130 - 145 mph" }
    ];
    facts = [
      "The Ford Mustang was officially introduced at the World's Fair in New York on April 17, 1964, and over 22,000 units were sold on the very first day.",
      "It was originally expected to sell about 100,000 units in its first year, but actually went on to sell over one million Mustangs within 18 months of launch.",
      "The car was named after the legendary P-51 Mustang fighter plane from World War II, rather than the wild horse, although the horse logo became its ultimate symbol."
    ];
  } else if (nameLower.includes("911") || nameLower.includes("porsche")) {
    history = `The Porsche 911 is the quintessential sports car, debuting in 1963 as a replacement for the 356. With its unique rear-engine layout and air-cooled flat-six, it achieved legendary status in endurance racing, rallies, and everyday road performance.

The 911's distinctive design silhouette—characterized by its sloping rear flyline and round headlights—has been preserved and meticulously evolved over more than six decades of production. It is widely considered by automotive purists and designers as one of the most successful, reliable, and beautifully engineered racing machines ever built.`;
    specs = [
      { label: "Engine Type", value: "Air-Cooled 2.0L - 3.6L Flat-Six" },
      { label: "Horsepower", value: "130 hp - 320 hp" },
      { label: "0-60 mph", value: "4.8 - 8.5 seconds" },
      { label: "Transmission", value: "5-Speed Manual Type 915 / G50" },
      { label: "Top Speed", value: "131 - 165 mph" }
    ];
    facts = [
      "The Porsche 911 was originally designated as the Porsche 901, but the name was changed to 911 after French automaker Peugeot objected, claiming ownership of three-digit car names with a zero in the middle.",
      "The iconic rear-engine placement puts the weight over the rear wheels, giving the 911 phenomenal traction and unique handling characteristics that skilled drivers exploit on race tracks.",
      "Over 70% of all Porsche 911 sports cars ever manufactured are still in road-worthy and running condition today, a testament to German engineering durability."
    ];
  } else if (nameLower.includes("skyline") || nameLower.includes("gtr") || nameLower.includes("gt-r")) {
    history = `The Nissan Skyline GT-R, nicknamed "Godzilla" by the automotive press, established absolute dominance in group-touring car racing. Known for its advanced all-wheel-drive systems, four-wheel steering, and legendary twin-turbocharged inline-six engines, it is a high-tech masterpiece of Japanese engineering.

From the classical third-generation (R32) that won every single JTCC race it entered, to the iconic fourth (R33) and fifth (R34) generations, the GT-R badge represented the pinnacle of high-tech racing performance. It became a global pop-culture icon through appearances in legendary racing simulators and blockbuster film franchises.`;
    specs = [
      { label: "Engine Type", value: "2.6L Twin-Turbocharged RB26DETT I6" },
      { label: "Horsepower", value: "276 hp (Officially) / ~320 hp (Actual)" },
      { label: "0-60 mph", value: "4.7 - 5.0 seconds" },
      { label: "Transmission", value: "5-Speed / 6-Speed Getrag Manual" },
      { label: "Top Speed", value: "155 mph (Limited) / 180 mph (Unrestricted)" }
    ];
    facts = [
      "The R32 GT-R earned its famous nickname \"Godzilla\" from the Australian magazine \"Wheels\" in 1989 due to its terrifyingly dominant performance in Australian touring car racing.",
      "Due to a Japanese manufacturers' voluntary \"gentlemen's agreement\", the twin-turbo RB26DETT engine was advertised as having 276 hp, but actual dyno testing revealed it shipped with over 320 hp.",
      "The legendary R34 Skyline GT-R featured an advanced multi-function display screen on the dashboard that showed real-time lap times, G-forces, and engine telemetry—a precursor to modern supercar displays."
    ];
  } else if (nameLower.includes("supra")) {
    history = `The Toyota Supra evolved from the Celica line into a legendary high-performance grand tourer. The fourth-generation (A80) Supra, powered by the bulletproof 2JZ-GTE twin-turbocharged inline-six, became an absolute titan of the tuner-car community and a pop-culture legend.

Known for its over-engineered block and internal components, the Supra's 2JZ engine could easily handle incredible amounts of turbo boost without requiring internal modifications. Coupled with its sleek 1990s organic design curves and massive active rear wing, the Supra remains one of the most sought-after Japanese classic sports cars in existence.`;
    specs = [
      { label: "Engine Type", value: "3.0L Twin-Turbocharged 2JZ-GTE I6" },
      { label: "Horsepower", value: "320 hp @ 5600 rpm" },
      { label: "0-60 mph", value: "4.6 seconds" },
      { label: "Transmission", value: "6-Speed V160 Manual" },
      { label: "Top Speed", value: "155 mph (Electronic Limit) / 177 mph" }
    ];
    facts = [
      "The fourth-generation Toyota Supra was so heavily focused on weight reduction that engineers used hollow carpet fibers, a hollow rear spoiler, and magnesium components to save weight.",
      "The cast-iron block of the legendary 2JZ-GTE engine is so robustly over-engineered that custom tuners have pushed it past 1,000 horsepower on stock internal engine components.",
      "An orange 1994 Toyota Supra played a starring role in the original \"The Fast and the Furious\" movie, catapulting the vehicle into mainstream global pop-culture super-stardom."
    ];
  } else if (nameLower.includes("camaro")) {
    history = `The Chevrolet Camaro was launched in September 1966 for the 1967 model year as Chevy's direct and aggressive answer to the sensational Ford Mustang. Spanning multiple generations of muscle and pony car legacy, the Camaro has always stood for high-displacement V8 options and iconic styling.

Special performance trim packages, such as the track-focused Z/28 and the high-horsepower SS, became instant legends of the Trans-Am racing series and drag strips alike. The Camaro represents the spirit of classic American horsepower and is highly treasured by muscle car enthusiasts worldwide.`;
    specs = [
      { label: "Engine Type", value: "4.9L V8 (Z/28) / 6.5L big-block V8 (SS)" },
      { label: "Horsepower", value: "290 hp - 375 hp" },
      { label: "0-60 mph", value: "5.3 - 6.8 seconds" },
      { label: "Transmission", value: "4-Speed Manual / 3-Speed Powerglide" },
      { label: "Top Speed", value: "120 - 135 mph" }
    ];
    facts = [
      "When asked by the automotive press what a 'Camaro' was, Chevrolet product managers famously replied that it was 'a small, vicious animal that eats Mustangs.'",
      "The high-performance track-oriented Z/28 package was originally designed to qualify the Camaro for the SCCA Trans-Am racing series, which capped displacement at 5.0 liters (302 cubic inches).",
      "To bypass corporate limits on engine size in compact cars, dealers like Yenko ordered Camaros via the Central Office Production Order (COPO) system with giant 7.0L (427 cu in) L72 V8 engines."
    ];
  } else if (nameLower.includes("corvette")) {
    history = `The Chevrolet Corvette is "America's Sports Car," introduced in 1953 as a sleek fiberglass-bodied roadster. Through generations of iconic designs like the C2 Sting Ray split-window, the muscle-heavy C3, and the modern mid-engined C8, it represents the high-water mark of American engineering, performance, and motorsports glory.

The Corvette pioneered light composite body panels and high-output V8 small-block engines, matching or exceeding European supercars at a fraction of the price. From Le Mans track wins to weekend cruising, it holds an unrivaled position in automotive collector history.`;
    specs = [
      { label: "Engine Type", value: "5.4L - 7.0L Small-Block V8 / LS-Series V8" },
      { label: "Horsepower", value: "300 hp - 435 hp / 650 hp (Z06)" },
      { label: "0-60 mph", value: "3.7 - 5.8 seconds" },
      { label: "Transmission", value: "4-Speed Manual / 6-Speed Manual" },
      { label: "Top Speed", value: "135 - 190 mph" }
    ];
    facts = [
      "The first Corvettes in 1953 were only available in Polo White with a Red interior and black soft top, and all 300 initial units were assembled by hand.",
      "The 1963 Corvette Sting Ray 'Split-Window' coupe is one of the most valuable classic American cars because the rear window divider was removed in 1964 due to driver visibility concerns.",
      "The Corvette became highly associated with astronauts in the 1960s after Florida Chevrolet dealer Jim Rathmann leased brand new Corvettes to NASA's Project Mercury astronauts for just $1 per year."
    ];
  } else if (nameLower.includes("miata")) {
    history = `The Mazda MX-5 Miata resurrected the classic British roadster concept with modern Japanese reliability, debuting at the Chicago Auto Show in 1989. With a perfect 50:50 weight distribution, rear-wheel drive, and lightweight design philosophy, it became the best-selling sports car in history.

Beloved for its sharp, direct steering, short-throw shifter, and low weight, the Miata proved that pure driving joy comes from agility and momentum, rather than raw horsepower. It remains the most raced car model in the world and is a cherished classic.`;
    specs = [
      { label: "Engine Type", value: "1.6L B6ZE(RS) DOHC I4 / 1.8L BP-ZE I4" },
      { label: "Horsepower", value: "115 hp @ 6500 rpm / 133 hp" },
      { label: "0-60 mph", value: "7.9 - 8.3 seconds" },
      { label: "Transmission", value: "5-Speed Manual" },
      { label: "Top Speed", value: "116 - 122 mph" }
    ];
    facts = [
      "The name 'Miata' comes from an Old High German word meaning 'reward' or 'prize,' highlighting Mazda's intention for it to be a rewarding driving experience.",
      "Mazda engineers designed the NA Miata's exhaust note to replicate the exact acoustic frequencies of vintage British roadsters like the Lotus Elan.",
      "The NA generation features iconic pop-up headlights that blink and wink, creating a friendly, expressive face that endeared it to millions of fans."
    ];
  } else {
    // Elegant generic fallback
    history = `The ${parsed.fullName} represents a significant chapter in automotive design. Characterized by its distinctive design language, calculated aerodynamics, and tailored engineering, this class of vehicle established major strides in consumer and collector appeal during its production run.

Appreciated by vintage purists, racing enthusiasts, and scale model cataloguers alike, this layout brings together technical sophistication and road-going elegance. It continues to be celebrated across international concours, historic rallies, and model showcase garages worldwide.`;
    specs = [
      { label: "Configuration", value: "Front-Engine / Rear-Wheel Drive" },
      { label: "Chassis Style", value: "Steel Monocoque / Space Frame" },
      { label: "Aspiration", value: "Naturally Aspirated" },
      { label: "Braking System", value: "Disc Brakes Front & Rear" },
      { label: "Collector Appeal", value: "Excellent Historical Heritage" }
    ];
    facts = [
      `The production run of this vehicle family was marked by several specialized modifications tailored to regional racing classes and rally configurations.`,
      `Pristine 1:1 scale examples of this design have seen steady appreciation in the vintage collector market, frequently drawing crowds at major enthusiast auctions.`,
      `Its design signature, with balanced proportions and a focus on driving engagement, has made it a favorite subject for scale model developers.`
    ];
  }

  return {
    detectedYear: year,
    detectedMake: make,
    detectedModel: model,
    history,
    specs,
    facts,
    collectorGuide: {
      collectabilityScore: score
    }
  };
}
