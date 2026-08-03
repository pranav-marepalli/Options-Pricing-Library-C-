import ExploreShell from "@/components/explore/ExploreShell";
import { fetchCurve } from "@/lib/api";
import {
  DEFAULT_AXIS,
  DEFAULT_PARAMS,
  DEFAULT_POINTS,
} from "@/lib/explore-bounds";

export default async function ExplorePage() {
  let initialCurve = null;

  try {
    initialCurve = await fetchCurve({
      ...DEFAULT_PARAMS,
      axis: DEFAULT_AXIS,
      points: DEFAULT_POINTS,
    });
  } catch {
    initialCurve = null;
  }

  return (
    <main className="min-h-full">
      <ExploreShell
        initialParams={DEFAULT_PARAMS}
        initialCurve={initialCurve}
      />
    </main>
  );
}
