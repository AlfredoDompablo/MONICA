-- CreateTable
CREATE TABLE "users" (
    "user_id" SERIAL NOT NULL,
    "full_name" VARCHAR(100) NOT NULL,
    "email" VARCHAR(100) NOT NULL,
    "password_hash" VARCHAR(255) NOT NULL,
    "role" VARCHAR(20) NOT NULL,
    "is_active" BOOLEAN NOT NULL DEFAULT true,

    CONSTRAINT "users_pkey" PRIMARY KEY ("user_id")
);

-- CreateTable
CREATE TABLE "nodes" (
    "node_id" VARCHAR(20) NOT NULL,
    "description" VARCHAR(100) NOT NULL,
    "latitude" DECIMAL(10,8) NOT NULL,
    "longitude" DECIMAL(11,8) NOT NULL,
    "last_seen" TIMESTAMP(6),
    "user_id" INTEGER,

    CONSTRAINT "nodes_pkey" PRIMARY KEY ("node_id")
);

-- CreateTable
CREATE TABLE "sensor_readings" (
    "reading_id" SERIAL NOT NULL,
    "node_id" VARCHAR(20) NOT NULL,
    "timestamp" TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "ph" DECIMAL(4,2),
    "dissolved_oxygen" DECIMAL(5,2),
    "turbidity" DECIMAL(6,2),
    "conductivity" DECIMAL(8,2),
    "temperature" DECIMAL(5,2),
    "battery_level" DECIMAL(4,2),

    CONSTRAINT "sensor_readings_pkey" PRIMARY KEY ("reading_id")
);

-- CreateTable
CREATE TABLE "waste_detections" (
    "detection_id" SERIAL NOT NULL,
    "node_id" VARCHAR(20) NOT NULL,
    "timestamp" TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "coverage_percent" DECIMAL(5,2),
    "image_data" BYTEA,
    "model_version" VARCHAR(50),
    "confidence" DECIMAL(3,2),

    CONSTRAINT "waste_detections_pkey" PRIMARY KEY ("detection_id")
);

-- CreateIndex
CREATE UNIQUE INDEX "users_email_key" ON "users"("email");

-- AddForeignKey
ALTER TABLE "nodes" ADD CONSTRAINT "nodes_user_id_fkey" FOREIGN KEY ("user_id") REFERENCES "users"("user_id") ON DELETE SET NULL ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "sensor_readings" ADD CONSTRAINT "sensor_readings_node_id_fkey" FOREIGN KEY ("node_id") REFERENCES "nodes"("node_id") ON DELETE RESTRICT ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "waste_detections" ADD CONSTRAINT "waste_detections_node_id_fkey" FOREIGN KEY ("node_id") REFERENCES "nodes"("node_id") ON DELETE RESTRICT ON UPDATE CASCADE;
