/*
  Warnings:

  - A unique constraint covering the columns `[key_hash]` on the table `nodes` will be added. If there are existing duplicate values, this will fail.

*/
-- AlterTable
ALTER TABLE "nodes" ADD COLUMN     "is_active" BOOLEAN NOT NULL DEFAULT true,
ADD COLUMN     "key_hash" VARCHAR(64);

-- CreateIndex
CREATE UNIQUE INDEX "nodes_key_hash_key" ON "nodes"("key_hash");
