import { useRef } from 'react'
import { useFrame } from '@react-three/fiber'
import * as THREE from 'three'

export default function Worm({ position, color, isActive, angle, facingRight = true }) {
  const groupRef = useRef()
  const bodyRef = useRef()
  const prevXRef = useRef(position[0])
  const walkTimeRef = useRef(0)
  const isMovingRef = useRef(false)

  useFrame((_, delta) => {
    if (!groupRef.current) return

    // 检测移动
    const dx = position[0] - prevXRef.current
    if (Math.abs(dx) > 0.01) {
      isMovingRef.current = true
      walkTimeRef.current = 0
    }
    prevXRef.current = position[0]
    walkTimeRef.current += delta

    // 移动动画持续 0.3s 后停止
    if (walkTimeRef.current > 0.3) {
      isMovingRef.current = false
    }

    // 朝向：朝左时沿 X 轴镜像
    groupRef.current.scale.set(facingRight ? 1 : -1, 1, 1)

    // 活跃时上下浮动 / 移动时走路晃动
    let yOffset = position[1]
    if (isActive) {
      yOffset += Math.sin(Date.now() * 0.004) * 0.8
    }
    if (isMovingRef.current) {
      yOffset += Math.abs(Math.sin(walkTimeRef.current * 20)) * 1.5
    }
    groupRef.current.position.y = yOffset

    // 炮管跟随相对角度
    if (bodyRef.current) {
      bodyRef.current.rotation.z = (angle - 90) * (Math.PI / 180)
    }
  })

  return (
    <group ref={groupRef} position={position}>
      {/* 地面阴影（固定在 terrain 高度，即 worm.y - 15） */}
      <mesh position={[0, -15, -0.1]} rotation={[-Math.PI / 2, 0, 0]}>
        <ringGeometry args={[4, 7, 16]} />
        <meshBasicMaterial
          color="#000000"
          transparent
          opacity={0.25}
          depthWrite={false}
          side={THREE.DoubleSide}
        />
      </mesh>

      {/* 身体 — 椭圆胶囊 */}
      <mesh castShadow position={[0, 3, 0.3]}>
        <capsuleGeometry args={[3.5, 8, 8, 16]} />
        <meshStandardMaterial color={color} roughness={0.5} metalness={0.15} side={THREE.DoubleSide} />
      </mesh>

      {/* 头部 */}
      <mesh castShadow position={[0, 8, 0.3]}>
        <sphereGeometry args={[4.5, 16, 16]} />
        <meshStandardMaterial color={color} roughness={0.4} metalness={0.1} side={THREE.DoubleSide} />
      </mesh>

      {/* 眼睛 */}
      <mesh position={[2.2, 9.5, 1.2]}>
        <sphereGeometry args={[1.4, 12, 12]} />
        <meshStandardMaterial color="white" roughness={0.2} />
      </mesh>
      <mesh position={[2.2, 9.5, 1.6]}>
        <sphereGeometry args={[0.6, 8, 8]} />
        <meshStandardMaterial color="black" roughness={0.2} />
      </mesh>

      <mesh position={[-2.2, 9.5, 1.2]}>
        <sphereGeometry args={[1.4, 12, 12]} />
        <meshStandardMaterial color="white" roughness={0.2} />
      </mesh>
      <mesh position={[-2.2, 9.5, 1.6]}>
        <sphereGeometry args={[0.6, 8, 8]} />
        <meshStandardMaterial color="black" roughness={0.2} />
      </mesh>

      {/* 炮管 — 跟随角度 */}
      <group ref={bodyRef} position={[0, 9, 0.3]}>
        <mesh castShadow position={[0, 3, 0]}>
          <cylinderGeometry args={[0.8, 1.0, 7, 8]} />
          <meshStandardMaterial color="#444" roughness={0.3} metalness={0.6} side={THREE.DoubleSide} />
        </mesh>
        {/* 炮口 */}
        <mesh position={[0, 6.5, 0]}>
          <sphereGeometry args={[1.1, 8, 8]} />
          <meshStandardMaterial color="#333" roughness={0.3} metalness={0.7} side={THREE.DoubleSide} />
        </mesh>
      </group>

      {/* 激活状态光晕 */}
      {isActive && (
        <mesh position={[0, 5, 0]}>
          <ringGeometry args={[6, 6.5, 32]} />
          <meshBasicMaterial color="#ffcc00" transparent opacity={0.6} side={THREE.DoubleSide} />
        </mesh>
      )}
    </group>
  )
}