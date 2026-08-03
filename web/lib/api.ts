export type OptionType = "call" | "put";

export type PriceRequest = {
  S0: number;
  K: number;
  r: number;
  q: number;
  sigma: number;
  T: number;
  type: OptionType;
};

export type PriceResponse = {
  price: number;
  delta: number;
  gamma: number;
  vega: number;
  theta: number;
};

export type ImpliedVolRequest = Omit<PriceRequest, "sigma"> & {
  target: number;
};

export type ImpliedVolResponse = {
  implied_vol: number;
};

export type CurveAxis = "S0" | "K" | "r" | "q" | "sigma" | "T";

export type CurvePoint = {
  x: number;
  price: number;
  delta: number;
  gamma: number;
  vega: number;
  theta: number;
};

export type CurveRequest = PriceRequest & {
  axis: CurveAxis;
  points?: number;
};

export type CurveResponse = {
  axis: CurveAxis;
  lo: number;
  hi: number;
  spot: PriceResponse;
  series: CurvePoint[];
};

function apiBase(): string {
  return (
    process.env.NEXT_PUBLIC_API_URL?.replace(/\/$/, "") ||
    "http://localhost:8080"
  );
}

async function postJson<T>(
  path: string,
  body: unknown,
  init?: { signal?: AbortSignal },
): Promise<T> {
  const res = await fetch(`${apiBase()}${path}`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
    signal: init?.signal,
  });

  if (!res.ok) {
    let detail = res.statusText;
    try {
      const err = (await res.json()) as { error?: string; message?: string };
      detail = err.error || err.message || detail;
    } catch {
      // keep statusText
    }
    throw new Error(detail || `Request failed (${res.status})`);
  }

  return res.json() as Promise<T>;
}

export function priceOption(body: PriceRequest): Promise<PriceResponse> {
  return postJson<PriceResponse>("/v1/price", body);
}

export function impliedVol(
  body: ImpliedVolRequest,
): Promise<ImpliedVolResponse> {
  return postJson<ImpliedVolResponse>("/v1/implied-vol", body);
}

export function fetchCurve(
  body: CurveRequest,
  init?: { signal?: AbortSignal },
): Promise<CurveResponse> {
  return postJson<CurveResponse>("/v1/curve", body, init);
}

/** Server-side price store entry (upserted by POST /v1/price). */
export type StoreEntry = PriceRequest &
  PriceResponse & {
    key: string;
    ts_ms: number;
  };

export type StoreLatestResponse = { entry: StoreEntry | null };
export type StoreListResponse = { count: number; entries: StoreEntry[] };

async function getJson<T>(path: string): Promise<T> {
  const res = await fetch(`${apiBase()}${path}`);
  if (!res.ok) {
    throw new Error(`Request failed (${res.status})`);
  }
  return res.json() as Promise<T>;
}

/** Latest price upserted into the shared server store (after Price option). */
export function fetchStoreLatest(): Promise<StoreLatestResponse> {
  return getJson<StoreLatestResponse>("/v1/store/latest");
}

export function fetchStoreRecent(limit = 10): Promise<StoreListResponse> {
  return getJson<StoreListResponse>(`/v1/store/recent?limit=${limit}`);
}

export function fetchStoreTop(limit = 10): Promise<StoreListResponse> {
  return getJson<StoreListResponse>(`/v1/store/top?limit=${limit}`);
}

export type StoreStatsResponse = {
  price_store: { size: number; max_entries: number; version: number };
  curve_cache: {
    size: number;
    max_entries: number;
    ttl_ms: number;
    hits: number;
    misses: number;
  };
};

export function fetchStoreStats(): Promise<StoreStatsResponse> {
  return getJson<StoreStatsResponse>("/v1/store/stats");
}
