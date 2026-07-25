import { useMemo, useRef } from 'react'
import * as THREE from 'three'

export default function Terrain({ heightMap }) {
  const meshRef = useRef()

  const { bodyGeo, grassGeo, rockGeo } = useMemo(() => {
    if (!heightMap || heightMap.length === 0) return {}

    const w = heightMap.length

    // === 地形主体（泥土层） ===
    const bodyShape = new THREE.Shape()
    bodyShape.moveTo(0, 0)
    for (let x = 0; x < w; x++) {
      bodyShape.lineTo(x, heightMap[x])
    }
    bodyShape.lineTo(w - 1, 0)
    bodyShape.closePath()

    const bodyGeometry = new THREE.ExtrudeGeometry(bodyShape, {
      steps: 1,
      depth: 22,
      bevelEnabled: true,
      bevelThickness: 2,
      bevelSize: 2,
      bevelSegments: 3,
    })

    // === 草皮表面 ===
    const grassShape = new THREE.Shape()
    grassShape.moveTo(0, heightMap[0])
    for (let x = 1; x < w; x++) {
      grassShape.lineTo(x, heightMap[x])
    }
    grassShape.lineTo(w - 1, heightMap[w - 1] - 4)
    grassShape.lineTo(0, heightMap[0] - 4)
    grassShape.closePath()

    const grassGeometry = new THREE.ShapeGeometry(grassShape)

    // === 岩石底层 ===
    const rockShape = new THREE.Shape()
    rockShape.moveTo(0, 0)
    for (let x = 0; x < w; x++) {
      rockShape.lineTo(x, Math.min(heightMap[x], 30))
    }
    rockShape.lineTo(w - 1, 0)
    rockShape.closePath()

    const rockGeometry = new THREE.ExtrudeGeometry(rockShape, {
      steps: 1,
      depth: 8,
      bevelEnabled: false,
    })

    return { bodyGeo: bodyGeometry, grassGeo: grassGeometry, rockGeo: rockGeometry }
  }, [heightMap])

  if (!bodyGeo) return null

  return (
    <group>
      {/* 岩石底层 */}
      <mesh
        geometry={rockGeo}
        position={[0, 0, -22]}
        receiveShadow
      >
        <meshStandardMaterial
          color="#3a3530"
          roughness={0.95}
          metalness={0.02}
        />
      </mesh>

      {/* 地形主体 — 泥土 */}
      <mesh
        ref={meshRef}
        geometry={bodyGeo}
        position={[0, 0, -22]}
        castShadow
        receiveShadow
      >
        <meshStandardMaterial
          color="#5c4033"
          roughness={0.85}
          metalness={0.05}
        />
      </mesh>

      {/* 地表草皮 */}
      <mesh
        geometry={grassGeo}
        position={[0, 0, 0.15]}
        receiveShadow
      >
        <meshStandardMaterial
          color="#4a7a2e"
          roughness={0.7}
          metalness={0.0}
        />
      </mesh>

      {/* 地表小草装饰点 */}
      <GrassTufts heightMap={heightMap} />
    </group>
  )
}

// 随机小草装饰
function GrassTufts({ heightMap }) {
  const positions = useMemo(() => {
    if (!heightMap || heightMap.length === 0) return null
    const pts = []
    // 每 30px 放一簇草
    for (let x = 0; x < heightMap.length; x += 30) {
      const jitter = (Math.random() - 0.5) * 20
      const cx = Math.max(5, Math.min(heightMap.length - 5, x + jitter))
      const cy = heightMap[Math.floor(cx)] + 1.5
      pts.push(cx, cy, 0.3)
    }
    return new Float32Array(pts)
  }, [heightMap])

  if (!positions) return null

  return (
    <points>
      <bufferGeometry>
        <bufferAttribute
          attach="attributes-position"
          count={positions.length / 3}
          array={positions}
          itemSize={3}
        />
      </bufferGeometry>
      <pointsMaterial
        size={3}
        color="#6aaa3a"
        depthWrite={false}
        transparent
        opacity={0.7}
      />
    </points>
  )
}