export interface Metadata {
  start: number;
  end: number;
  nested?: unknown;
}

export function adjustMetadata(meta: unknown, offset: number): unknown {
  if (meta == null || typeof meta !== 'object') return null;
  if (Array.isArray(meta)) {
    return meta.map(item => adjustMetadata(item, offset));
  }
  const adjusted: Record<string, unknown> = {};
  if ('start' in meta && typeof (meta as any).start === 'number' &&
      'end' in meta && typeof (meta as any).end === 'number') {
    adjusted.start = (meta as any).start - offset;
    adjusted.end = (meta as any).end - offset;
  }
  for (const [key, value] of Object.entries(meta)) {
    if (key === 'start' || key === 'end') continue;
    const valAdj = adjustMetadata(value, offset);
    if (valAdj !== null) {
      adjusted[key] = valAdj;
    }
  }
  return Object.keys(adjusted).length > 0 ? adjusted : null;
}
