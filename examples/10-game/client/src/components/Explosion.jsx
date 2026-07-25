import { useRef, useMemo, useState } from 'react'
import { useFrame } from '@react-three/fiber'
import * as THREE from 'three'

export default function Explosion({ position, radius }) {
  const groupRef = useRef()
  const timeRef = useRef(0)
  const [done, setDone] = useState(false)

  useFrame((_, delta) => {
    timeRef.current += delta
    if (timeRef.current > 3.0 && !done) setDone(true)
  })

  if (done) return null

  return (
    <group ref={groupRef} position={position}>
      <FlashSphere timeRef={timeRef} radius={radius} />
      <SmokeCloud timeRef={timeRef} radius={radius} />
      <FireParticles timeRef={timeRef} radius={radius} />
      <GroundFlash timeRef={timeRef} radius={radius} />
    </group>
  )
}

// === 中心闪光球（大而亮） ===
function FlashSphere({ timeRef, radius }) {
  const meshRef = useRef()
  const r = radius * 0.6

  useFrame(() => {
    if (!meshRef.current) return
    const t = timeRef.current
    const scale = 0.5 + t * 12
    meshRef.current.scale.setScalar(scale)
    meshRef.current.material.opacity = Math.max(0, 1 - t * 4)
    meshRef.current.visible = t < 0.4
  })

  return (
    <mesh ref={meshRef}>
      <sphereGeometry args={[r, 16, 16]} />
      <meshBasicMaterial
        color="#ffffff"
        transparent
        opacity={1}
        depthWrite={false}
        blending={THREE.AdditiveBlending}
      />
    </mesh>
  )
}

// === 烟雾云（多个膨胀球体） ===
function SmokeCloud({ timeRef, radius }) {
  const groupRef = useRef()
  const r = radius * 0.5

  const spheres = useMemo(() => {
    const s = []
    for (let i = 0; i < 8; i++) {
      s.push({
        offset: [
          (Math.random() - 0.5) * r * 0.6,
          (Math.random() - 0.5) * r * 0.6,
          (Math.random() - 0.5) * r * 0.3,
        ],
        phase: Math.random() * 0.3,
        size: r * (0.5 + Math.random() * 0.5),
      })
    }
    return s
  }, [r])

  useFrame(() => {
    if (!groupRef.current) return
    const t = timeRef.current
    groupRef.current.children.forEach((child, i) => {
      if (!child) return
      const s = spheres[i]
      const localT = Math.max(0, t - s.phase)
      const scale = 0.3 + localT * 4
      const opacity = Math.max(0, (1 - localT * 0.7) * 0.5)
      child.scale.setScalar(scale)
      child.material.opacity = opacity
      child.visible = localT < 1.5
    })
  })

  return (
    <group ref={groupRef}>
      {spheres.map((s, i) => (
        <mesh key={i} position={s.offset}>
          <sphereGeometry args={[s.size, 8, 8]} />
          <meshBasicMaterial
            color="#ff8830"
            transparent
            opacity={0.5}
            depthWrite={false}
            blending={THREE.AdditiveBlending}
          />
        </mesh>
      ))}
    </group>
  )
}

// === 地面冲击闪光（水平圆盘） ===
function GroundFlash({ timeRef, radius }) {
  const meshRef = useRef()
  const r = radius * 0.7

  useFrame(() => {
    if (!meshRef.current) return
    const t = timeRef.current
    const scale = 0.3 + t * 6
    meshRef.current.scale.set(scale, scale, 1)
    meshRef.current.material.opacity = Math.max(0, (1 - t * 2) * 0.7)
    meshRef.current.visible = t < 0.8
  })

  return (
    <mesh ref={meshRef} rotation={[-Math.PI / 2, 0, 0]} position={[0, 0, -0.3]}>
      <ringGeometry args={[r * 0.3, r, 32]} />
      <meshBasicMaterial
        color="#ffcc44"
        transparent
        opacity={0.7}
        depthWrite={false}
        side={THREE.DoubleSide}
        blending={THREE.AdditiveBlending}
      />
    </mesh>
  )
}

// === 火焰粒子 ===
function FireParticles({ timeRef, radius }) {
  const pointsRef = useRef()
  const particleCount = 300

  const { positions, velocities, colors } = useMemo(() => {
    const pos = new Float32Array(particleCount * 3)
    const vel = new Float32Array(particleCount * 3)
    const col = new Float32Array(particleCount * 3)

    for (let i = 0; i < particleCount; i++) {
      const angle = Math.random() * Math.PI * 2
      const phi = Math.random() * Math.PI * 0.8
      const speed = Math.random() * 120 + 40

      vel[i * 3] = Math.cos(angle) * Math.cos(phi) * speed
      vel[i * 3 + 1] = Math.sin(phi) * speed + 60
      vel[i * 3 + 2] = (Math.random() - 0.5) * 40

      pos[i * 3] = (Math.random() - 0.5) * 4
      pos[i * 3 + 1] = Math.random() * 3
      pos[i * 3 + 2] = (Math.random() - 0.5) * 4

      const t = Math.random()
      if (t < 0.2) {
        col[i * 3] = 1; col[i * 3 + 1] = 0.95; col[i * 3 + 2] = 0.8
      } else if (t < 0.5) {
        col[i * 3] = 1; col[i * 3 + 1] = 0.5 + t * 0.5; col[i * 3 + 2] = 0.05
      } else if (t < 0.8) {
        col[i * 3] = 0.9; col[i * 3 + 1] = 0.2; col[i * 3 + 2] = 0.02
      } else {
        col[i * 3] = 0.4; col[i * 3 + 1] = 0.1; col[i * 3 + 2] = 0.02
      }
    }
    return { positions: pos, velocities: vel, colors: col }
  }, [radius])

  useFrame((_, delta) => {
    if (!pointsRef.current) return
    const t = timeRef.current
    const geo = pointsRef.current.geometry
    const posArr = geo.attributes.position.array
    const colArr = geo.attributes.color.array
    const gravity = 150

    for (let i = 0; i < particleCount; i++) {
      const idx = i * 3
      posArr[idx] += velocities[idx] * delta
      posArr[idx + 1] += velocities[idx + 1] * delta - gravity * delta * (0.3 + t * 0.3)
      posArr[idx + 2] += velocities[idx + 2] * delta

      colArr[idx] = Math.max(0, colArr[idx] - delta * 0.5)
      colArr[idx + 1] = Math.max(0, colArr[idx + 1] - delta * 0.6)
      colArr[idx + 2] = Math.max(0, colArr[idx + 2] - delta * 0.4)
    }

    geo.attributes.position.needsUpdate = true
    geo.attributes.color.needsUpdate = true
    pointsRef.current.material.opacity = Math.max(0, 1 - t / 2.5)
    pointsRef.current.visible = t < 3.0
  })

  return (
    <points ref={pointsRef}>
      <bufferGeometry>
        <bufferAttribute
          attach="attributes-position"
          count={particleCount}
          array={positions}
          itemSize={3}
        />
        <bufferAttribute
          attach="attributes-color"
          count={particleCount}
          array={colors}
          itemSize={3}
        />
      </bufferGeometry>
      <pointsMaterial
        size={3.5}
        vertexColors
        blending={THREE.AdditiveBlending}
        depthWrite={false}
        transparent
        sizeAttenuation
      />
    </points>
  )
}