import { useRef, useEffect, useState, useMemo } from 'react'
import { Canvas, useFrame } from '@react-three/fiber'
import { OrbitControls, Sky } from '@react-three/drei'
import Terrain from './Terrain'
import Worm from './Worm'
import Explosion from './Explosion'

function Scene({ gameState, trajectory }) {
  const terrain = gameState?.terrain || []
  const worms = gameState?.worms || []
  const explosions = gameState?.explosions || []
  const wind = gameState?.wind || { direction: 0, strength: 0 }

  return (
    <group>
      {/* 光照 */}
      <directionalLight
        position={[200, 400, 100]}
        intensity={2.5}
        color="#ffe8c0"
        castShadow
        shadow-mapSize-width={1024}
        shadow-mapSize-height={1024}
        shadow-camera-left={-100}
        shadow-camera-right={900}
        shadow-camera-top={600}
        shadow-camera-bottom={-50}
      />
      <ambientLight intensity={0.5} color="#8899cc" />
      <hemisphereLight args={["#ffccaa", "#445588", 0.4]} />

      {/* 天空 */}
      <Sky
        distance={3000}
        sunPosition={[200, 400, 100]}
        inclination={0.5}
        azimuth={0.25}
      />

      {/* 地形 */}
      <Terrain heightMap={terrain} />

      {/* 虫 */}
      {worms.map((worm, i) => (
        worm.alive && (
          <Worm
            key={i}
            position={[worm.x, worm.y, 0.15]}
            color={WORM_COLORS[worm.color % WORM_COLORS.length]}
            isActive={i === gameState?.currentTurn}
            angle={worm.angle}
            facingRight={worm.facingRight !== false}
          />
        )
      ))}

      {/* 爆炸 */}
      {explosions.map((exp, i) => (
        <Explosion
          key={i}
          position={[exp.cx, exp.cy, 0.5]}
          radius={exp.radius}
        />
      ))}

      {/* 弹道轨迹 + 动画炮弹 */}
      <Projectile trajectory={trajectory} />

      {/* 风力指示线 */}
      {wind.strength > 0.05 && (
        <WindIndicator wind={wind} />
      )}
    </group>
  )
}

const WORM_COLORS = ['#ff4444', '#44aaff', '#ffaa00', '#44ff44']

// ===== 动画炮弹 + 轨迹线 =====
function Projectile({ trajectory }) {
  const [active, setActive] = useState(false)
  const ballRef = useRef()
  const progressRef = useRef(0)
  const pointsRef = useRef(null)

  // 每次收到新轨迹时重置动画
  useEffect(() => {
    if (!trajectory || trajectory.length < 2) {
      console.log('[Projectile] 轨迹无效, 清空动画', trajectory?.length)
      setActive(false)
      return
    }
    console.log('[Projectile] 开始动画, 轨迹点数:', trajectory.length,
      '首点:', JSON.stringify(trajectory[0]),
      '尾点:', JSON.stringify(trajectory[trajectory.length - 1]))
    pointsRef.current = trajectory
    progressRef.current = 0
    setActive(true)
  }, [trajectory])

  useFrame((_, delta) => {
    if (!active) return

    const pts = pointsRef.current
    if (!pts || pts.length < 2) {
      setActive(false)
      return
    }

    // 动画速度：约 1.5 秒飞完
    const speed = 1.0 / (pts.length * 0.006)
    progressRef.current += delta * speed

    const idx = Math.floor(progressRef.current * (pts.length - 1))
    const clampedIdx = Math.min(idx, pts.length - 1)
    const pt = pts[clampedIdx]

    if (progressRef.current >= 1.0) {
      progressRef.current = 1.0
      console.log('[Projectile] 动画结束, 最终位置:', pt.x, pt.y)
      setActive(false)
    }

    if (ballRef.current) {
      ballRef.current.position.set(pt.x, pt.y, 0.5)
    }
  })

  if (!trajectory || trajectory.length < 2) return null

  // 静态弹道预览线
  const fullCurve = trajectory.map(p => [p.x, p.y, 0.35])

  return (
    <group>
      {/* 弹道预览线 */}
      <line>
        <bufferGeometry>
          <bufferAttribute
            attach="attributes-position"
            count={fullCurve.length}
            array={new Float32Array(fullCurve.flat())}
            itemSize={3}
          />
        </bufferGeometry>
        <lineBasicMaterial
          color="#ff8800"
          transparent
          opacity={0.35}
          depthTest={false}
        />
      </line>

      {/* 飞行炮弹 */}
      {active && (
        <mesh ref={ballRef} position={[trajectory[0].x, trajectory[0].y, 0.5]}>
          <sphereGeometry args={[2.5, 16, 16]} />
          <meshStandardMaterial
            color="#ff4444"
            roughness={0.2}
            metalness={0.5}
            emissive="#ff2200"
            emissiveIntensity={0.6}
          />
        </mesh>
      )}
    </group>
  )
}

function WindIndicator({ wind }) {
  const arrowX = 700
  const arrowY = 400
  const arrowLen = Math.abs(wind.strength) * 100

  const points = useMemo(() => {
    const dir = wind.direction > 0 ? 1 : -1
    return [
      [arrowX - dir * arrowLen, arrowY, 0.2],
      [arrowX + dir * arrowLen, arrowY, 0.2],
    ]
  }, [wind])

  return (
    <group>
      <line>
        <bufferGeometry>
          <bufferAttribute
            attach="attributes-position"
            count={2}
            array={new Float32Array(points.flat())}
            itemSize={3}
          />
        </bufferGeometry>
        <lineBasicMaterial color="#88ccff" linewidth={2} transparent opacity={0.6} />
      </line>
      <mesh position={[arrowX + (wind.direction > 0 ? 1 : -1) * arrowLen, arrowY, 0.2]}>
        <coneGeometry args={[4, 8, 6]} />
        <meshBasicMaterial color="#88ccff" transparent opacity={0.6} />
      </mesh>
    </group>
  )
}

export default function GameCanvas({ gameState, trajectory }) {
  return (
    <Canvas
      camera={{ position: [400, 280, 350], fov: 45, near: 1, far: 3000 }}
      style={{ position: 'absolute', top: 0, left: 0, width: '100%', height: '100%' }}
      gl={{ antialias: true, alpha: false }}
      shadows
    >
      <color attach="background" args={['#1a1a3e']} />
      <Scene gameState={gameState} trajectory={trajectory} />
      <OrbitControls
        enableRotate={false}
        enablePan={true}
        enableZoom={true}
        target={[400, 200, 0]}
        maxPolarAngle={Math.PI / 2.2}
        minDistance={200}
        maxDistance={800}
      />
    </Canvas>
  )
}