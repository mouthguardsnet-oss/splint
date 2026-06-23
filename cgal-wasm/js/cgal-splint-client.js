export async function createCgalSplintClient(createModule) {
  const module = await createModule();

  function allocF64(values) {
    const data = values instanceof Float64Array ? values : new Float64Array(values);
    const ptr = module._malloc(data.byteLength);
    module.HEAPF64.set(data, ptr / 8);
    return { ptr, count: data.length };
  }

  function allocU32(values) {
    const data = values instanceof Uint32Array ? values : new Uint32Array(values);
    const ptr = module._malloc(data.byteLength);
    module.HEAPU32.set(data, ptr / 4);
    return { ptr, count: data.length };
  }

  function process({ positions, indices, margin }) {
    const pos = allocF64(positions);
    const idx = allocU32(indices);
    const mar = allocF64(margin || []);

    try {
      const ok = module._cgal_process(pos.ptr, pos.count, idx.ptr, idx.count, mar.ptr, mar.count);
      const report = module.UTF8ToString(module._cgal_get_report_ptr());
      const projectedCount = module._cgal_get_projected_count();
      const projectedPtr = module._cgal_get_projected_ptr();
      const projected = projectedCount > 0 && projectedPtr
        ? new Float64Array(module.HEAPF64.buffer, projectedPtr, projectedCount).slice()
        : new Float64Array();

      return {
        ok: ok === 1,
        report,
        projected,
        borderHalfedges: module._cgal_get_border_halfedge_count(),
        invalidFaces: module._cgal_get_invalid_face_count(),
        selfIntersections: module._cgal_get_self_intersection_count()
      };
    } finally {
      module._free(pos.ptr);
      module._free(idx.ptr);
      module._free(mar.ptr);
    }
  }

  return { module, process };
}
